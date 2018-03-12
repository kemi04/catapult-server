#include "Scheduler.h"
#include "FutureUtils.h"
#include "IoServiceThreadPool.h"
#include "StrandOwnerLifetimeExtender.h"
#include "catapult/utils/Logging.h"
#include "catapult/utils/WeakContainer.h"
#include "catapult/exceptions.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>

namespace catapult { namespace thread {

	namespace {
		std::chrono::milliseconds ToMillis(const utils::TimeSpan& timeSpan) {
			return std::chrono::milliseconds(timeSpan.millis());
		}

		/// Wraps a task using an implicit strand.
		template<typename TCallbackWrapper>
		class BasicTaskWrapper {
		public:
			BasicTaskWrapper(boost::asio::io_service& service, const Task& task, TCallbackWrapper& wrapper)
					: m_task(task)
					, m_wrapper(wrapper)
					, m_timer(service, ToMillis(task.StartDelay))
					, m_isStopped(false) {
				CATAPULT_LOG(debug) << "task '" << m_task.Name << "' is scheduled in " << task.StartDelay;
			}

		public:
			void start() {
				startWait();
			}

			void stop() {
				m_isStopped = true;
				m_timer.cancel();
			}

		private:
			void startWait() {
				if (m_isStopped) {
					CATAPULT_LOG(trace) << "bypassing start of stopped timer";
					return;
				}

				m_timer.async_wait(m_wrapper.wrap([this](const auto& ec) { this->handleWait(ec); }));
			}

			void handleWait(const boost::system::error_code& ec) {
				if (ec) {
					if (boost::asio::error::operation_aborted == ec)
						return;

					CATAPULT_THROW_EXCEPTION(boost::system::system_error(ec));
				}

				m_task.Callback().then(m_wrapper.wrapFutureContinuation([this](auto result) { this->handleCompletion(result); }));
			}

		void handleCompletion(TaskResult result) {
			if (result == TaskResult::Break) {
				CATAPULT_LOG(warning) << "task '" << m_task.Name << "' broke and will be stopped";
				return;
			}

			CATAPULT_LOG(trace) << "task '" << m_task.Name << "' will continue in " << m_task.RepeatDelay;
			m_timer.expires_from_now(ToMillis(m_task.RepeatDelay));
			startWait();
		}

		private:
			Task m_task;
			TCallbackWrapper& m_wrapper;
			boost::asio::steady_timer m_timer;
			bool m_isStopped;
		};

		/// Wraps a task using using an explicit strand and ensures deterministic shutdown by using
		/// enable_shared_from_this.
		class StrandedTaskWrapper : public std::enable_shared_from_this<StrandedTaskWrapper> {
		public:
			StrandedTaskWrapper(boost::asio::io_service& service, const Task& task)
					: m_strand(service)
					, m_strandWrapper(m_strand)
					, m_impl(service, task, *this)
			{}

		public:
			void start() {
				post([](auto& impl) { impl.start(); });
			}

			void stop() {
				post([](auto& impl) { impl.stop(); });
			}

		public:
			template<typename THandler>
			auto wrap(THandler handler) {
				return m_strandWrapper.wrap(shared_from_this(), handler);
			}

			template<typename THandler>
			auto wrapFutureContinuation(THandler handler) {
				// boost asio callbacks need to be copyable, so they do not support move-only arguments (e.g. future)
				// as a workaround, call future.get() outside of the strand and post the result onto the strand
				return [pThis = shared_from_this(), handler](auto&& future) {
					return pThis->m_strandWrapper.wrap(pThis, handler)(future.get());
				};
			}

		private:
			template<typename THandler>
			void post(THandler handler) {
				return m_strandWrapper.post(shared_from_this(), [handler](const auto& pThis) {
					handler(pThis->m_impl);
				});
			}

		private:
			boost::asio::strand m_strand;
			StrandOwnerLifetimeExtender<StrandedTaskWrapper> m_strandWrapper;
			BasicTaskWrapper<StrandedTaskWrapper> m_impl;
		};

		class DefaultScheduler
				: public Scheduler
				, public std::enable_shared_from_this<DefaultScheduler> {
		public:
			explicit DefaultScheduler(const std::shared_ptr<IoServiceThreadPool>& pPool)
					: m_pPool(pPool)
					, m_service(pPool->service())
					, m_numExecutingTaskCallbacks(0)
					, m_isStopped(false)
					, m_tasks([](auto& task) { task.stop(); })
			{}

			~DefaultScheduler() { shutdown(); }

		public:
			uint32_t numScheduledTasks() const override {
				return static_cast<uint32_t>(m_tasks.size());
			}

			uint32_t numExecutingTaskCallbacks() const override {
				return m_numExecutingTaskCallbacks;
			}

		public:
			void addTask(const Task& task) override {
				if (m_isStopped)
					CATAPULT_THROW_RUNTIME_ERROR("cannot add new scheduled task because scheduler has shutdown");

				// wrap the task callback to automatically update m_numExecutingTaskCallbacks
				auto taskCopy = task;
				taskCopy.Callback = [pThis = shared_from_this(), callback = task.Callback]() {
					++pThis->m_numExecutingTaskCallbacks;
					return compose(callback(), [pThis](auto&& resultFuture) {
						--pThis->m_numExecutingTaskCallbacks;
						return std::move(resultFuture);
					});
				};

				auto pTask = std::make_shared<StrandedTaskWrapper>(m_service, taskCopy);
				m_tasks.insert(pTask);
				pTask->start();
			}

			void shutdown() override {
				bool expectedIsStopped = false;
				if (!m_isStopped.compare_exchange_strong(expectedIsStopped, true))
					return;

				CATAPULT_LOG(trace) << "Scheduler stopping";
				m_tasks.clear();
				CATAPULT_LOG(info) << "Scheduler stopped";
			}

		private:
			std::shared_ptr<const IoServiceThreadPool> m_pPool;
			boost::asio::io_service& m_service;

			std::atomic<uint32_t> m_numExecutingTaskCallbacks;
			std::atomic_bool m_isStopped;
			utils::WeakContainer<StrandedTaskWrapper> m_tasks;
		};
	}

	std::shared_ptr<Scheduler> CreateScheduler(const std::shared_ptr<IoServiceThreadPool>& pPool) {
		auto pScheduler = std::make_shared<DefaultScheduler>(pPool);
		return std::move(pScheduler);
	}
}}
