using Services;
using System.Timers;

namespace Hooks
{
    internal class HardwareActivityHook
    {
        public delegate void BatteryHandler(string level);
        public delegate void SpaceAvailableInDriveHandler(long freeSpace);
        public delegate void AnyFullscreenApplicationHandler(bool fullscreen);
        public event BatteryHandler OnBattery;
        public event SpaceAvailableInDriveHandler OnSpaceAvailable;
        public event AnyFullscreenApplicationHandler OnAnyApplicationFullscrenStatusChanged;

        private bool lastAnyApplicationFullscreenStatus = false;

        private TimerService timerService = new TimerService();
        private HardwareService hardwareService = new HardwareService();

        public HardwareActivityHook() 
        {
            this.timerService.CreateTimer(1000, OnBatteryEvent, true, true);
            this.timerService.CreateTimer(1000, OnSpaceAvailableInDrivesEvent, true, true);
            this.timerService.CreateTimer(1000, OnAnyApplicationFullscreenStatusEvent, true, true);
        }

        private void OnBatteryEvent(object sender, ElapsedEventArgs e)
        {
            string batteryInfo = this.hardwareService.GetBatteryInfo();
            OnBattery.Invoke(batteryInfo);
        }

        private void OnSpaceAvailableInDrivesEvent(object sender, ElapsedEventArgs e)
        {
            long freeSpace = this.hardwareService.GetFreeSpaceAvailableInDrive("C");
            OnSpaceAvailable.Invoke(freeSpace);
        }

        private void OnAnyApplicationFullscreenStatusEvent(object sender, ElapsedEventArgs e)
        {
            bool fullscreenStatus = HardwareService.isAnyApplicationFullscreen();

            if (fullscreenStatus != this.lastAnyApplicationFullscreenStatus)
            {
                this.lastAnyApplicationFullscreenStatus = fullscreenStatus;
                OnAnyApplicationFullscrenStatusChanged.Invoke(fullscreenStatus);
            }
        }
    }
}
