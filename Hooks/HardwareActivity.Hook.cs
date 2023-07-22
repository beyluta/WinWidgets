using Services;
using System;
using System.Timers;

namespace Hooks
{
    internal class HardwareActivityHook
    {
        public delegate void BatteryLevelHandler(string level);
        public delegate void SpaceAvailableInDriveHandler(long freeSpace);
        public delegate void DeviceTemperatureHandler(double temperature);
        public event BatteryLevelHandler OnBatteryLevel;
        public event SpaceAvailableInDriveHandler OnSpaceAvailable;
        public event DeviceTemperatureHandler OnDeviceTemperature;

        private TimerService timerService = new TimerService();
        private HardwareService hardwareService = new HardwareService();

        public HardwareActivityHook() 
        {
            this.timerService.CreateTimer(1000, OnBatteryLevelEvent, true, true);
            this.timerService.CreateTimer(1000, OnSpaceAvailableInDrivesEvent, true, true);
            this.timerService.CreateTimer(1000, OnDeviceTemperatureEvent, true, true);
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

        private void OnDeviceTemperatureEvent(object sender, ElapsedEventArgs e)
        {
            double temperature = this.hardwareService.GetDeviceTemperature();
            OnDeviceTemperature.Invoke(temperature);
        }
    }
}
