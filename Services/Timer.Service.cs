using System.Timers;

namespace Services
{
    internal class TimerService
    {
        public void CreateTimer(int miliseconds, ElapsedEventHandler elapsedEventHandler, bool autoReset, bool enabled)
        {
            Timer timer = new Timer(miliseconds);
            timer.Elapsed += elapsedEventHandler;
            timer.AutoReset = autoReset;
            timer.Enabled = enabled;
        }
    }
}
