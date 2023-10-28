using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

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
        /// Calls the WidgetsDotNetCore DLL and checks if any application using DirectX is fullscreen
        /// </summary>
        /// <returns>Whether any DirectX application is fullscreen</returns>
        [DllImport(@"WidgetsDotNetCore.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool isAnyApplicationFullscreen();
    }
}
