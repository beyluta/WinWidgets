using System.Timers;

namespace Services
{
    internal class TimerService
    {
        /// <summary>
        /// Creates and automatically starts a timer
        /// </summary>
        /// <param name="milliseconds">milliseconds until the next tick</param>
        /// <param name="elapsedEventHandler">Event handler that will be called every tick</param>
        /// <param name="autoReset">should timer be automatically reset</param>
        /// <param name="enabled">should timer be enabled</param>
        /// <returns>The timer instance</returns>
        public Timer CreateTimer(int milliseconds, ElapsedEventHandler elapsedEventHandler, bool autoReset, bool enabled)
        {
            Timer timer = new Timer(milliseconds);
            timer.Elapsed += elapsedEventHandler;
            timer.AutoReset = autoReset;
            timer.Enabled = enabled;
            return timer;
        }
    }
}
