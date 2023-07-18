using Services;
using System.Timers;

namespace Hooks
{
    internal class HardwareActivityHook
    {
        public delegate void BatteryLevelHandler(string level);
        public delegate void SpaceAvailableInDriveHandler(long freeSpace);
        public event BatteryLevelHandler BatteryLevel;
        public event SpaceAvailableInDriveHandler SpaceAvailableInDrive;

        private HardwareService hardwareService = new HardwareService();

        public HardwareActivityHook() 
        {
            CreateTimer(1000, OnBatteryLevelEvent, true, true);
            CreateTimer(1000, OnSpaceAvailableInDrivesEvent, true, true);
        }

        private void CreateTimer(int miliseconds, ElapsedEventHandler elapsedEventHandler, bool autoReset, bool enabled)
        {
            Timer timer = new Timer(miliseconds);
            timer.Elapsed += elapsedEventHandler;
            timer.AutoReset = autoReset;
            timer.Enabled = enabled;
        }

        private void OnBatteryLevelEvent(object sender, ElapsedEventArgs e)
        {
            string level = this.hardwareService.GetBatteryLevelPercentage();
            BatteryLevel.Invoke(level);
        }

        private void OnSpaceAvailableInDrivesEvent(object sender, ElapsedEventArgs e)
        {
            long freeSpace = this.hardwareService.GetFreeSpaceAvailableInDrive("C");
            SpaceAvailableInDrive.Invoke(freeSpace);
        }
    }
}
