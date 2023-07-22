using Services;
using System.Timers;

namespace Hooks
{
    internal class HardwareActivityHook
    {
        public delegate void BatteryLevelHandler(string level);
        public delegate void SpaceAvailableInDriveHandler(long freeSpace);
        public event BatteryLevelHandler OnBatteryLevel;
        public event SpaceAvailableInDriveHandler OnSpaceAvailable;

        private TimerService timerService = new TimerService();
        private HardwareService hardwareService = new HardwareService();

        public HardwareActivityHook() 
        {
            this.timerService.CreateTimer(1000, OnBatteryLevelEvent, true, true);
            this.timerService.CreateTimer(1000, OnSpaceAvailableInDrivesEvent, true, true);
        }

        private void OnBatteryLevelEvent(object sender, ElapsedEventArgs e)
        {
            string level = this.hardwareService.GetBatteryLevelPercentage();
            OnBatteryLevel.Invoke(level);
        }

        private void OnSpaceAvailableInDrivesEvent(object sender, ElapsedEventArgs e)
        {
            long freeSpace = this.hardwareService.GetFreeSpaceAvailableInDrive("C");
            OnSpaceAvailable.Invoke(freeSpace);
        }
    }
}
