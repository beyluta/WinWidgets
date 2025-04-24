using FullScreenDetection;
using Models;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Services
{
    internal class HardwareService
    {
        private FullscreenDetecter detector = new FullscreenDetecter();

        /// <summary>
        /// Get battery info
        /// </summary>
        /// <returns>Battery info</returns>
        public string GetBatteryInfo()
        {
            PowerStatus powerStatus = SystemInformation.PowerStatus;

            BatteryInfo batteryInfo = new BatteryInfo()
            { 
                batteryChargeStatus = powerStatus.BatteryChargeStatus.ToString(),
                batteryFullLifetime = powerStatus.BatteryFullLifetime.ToString(),
                batteryLifePercent = (powerStatus.BatteryLifePercent * 100).ToString(),
                batteryLifeRemaining = powerStatus.BatteryLifeRemaining.ToString(),
                powerLineStatus = powerStatus.PowerLineStatus.ToString(),
            };

            return JsonConvert.SerializeObject(batteryInfo);
        }

        /// <summary>
        /// Gets the amount of free space available in a specific drive
        /// </summary>
        /// <param name="driveLetter">Letter(s) of the desired drive</param>
        /// <returns>Space represented in bytes</returns>
        public long GetFreeSpaceAvailableInDrive(string driveLetter)
        {
            return (new DriveInfo(driveLetter)).AvailableFreeSpace;
        }

        /// <summary>
        /// Checks if any application is fullscreen
        /// </summary>
        /// <returns>Whether any application is fullscreen</returns>
        public Task<bool> isAnyApplicationFullscreenAsync()
        {
            return Task.Run(() =>
            {
                try
                {
                    var list = detector.DetectFullscreenApplication();
                    return list != null && list.Count > 1;
                }
                catch
                {
                    return false;
                }
            });
        }

    }
}
