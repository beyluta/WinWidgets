using System.IO;
using System.Windows.Forms;
using System.Management;
using System;
using Models;

namespace Services
{
    internal class HardwareService
    {
        /// <summary>
        /// Get battery level in percentage
        /// </summary>
        /// <returns>Battery level in percent</returns>
        public string GetBatteryLevelPercentage()
        {
            PowerStatus powerStatus = SystemInformation.PowerStatus;
            return powerStatus.BatteryLifePercent.ToString();
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
        /// Gets the temperature the device
        /// </summary>
        /// <returns>The temperature the device</returns>
        public double GetDeviceTemperature(HardwareMetrics hardwareMetrics = HardwareMetrics.Celcius)
        {
            double temperature = 0;

            ManagementObjectSearcher searcher = new ManagementObjectSearcher(@"root\WMI", "SELECT * FROM MSAcpi_ThermalZoneTemperature");
            foreach (ManagementObject obj in searcher.Get())
            {
                temperature = Convert.ToDouble(obj["CurrentTemperature"].ToString());

                if (hardwareMetrics == HardwareMetrics.Celcius)
                {
                    temperature = (temperature - 2732) / 10.0;
                }
            }

            return temperature;
        }
    }
}
