namespace Models
{
    internal enum HardwareEvent
    {
        NativeKeys = 0,
        Battery = 1,
        SpaceAvailable = 2,
    }

    internal struct PeripheralAction
    {
        /// <summary>
        /// Keycode of the key that was pressed
        /// </summary>
        public string keycode { get; set; }

        /// <summary>
        /// State of the key. Example: keydown, keyup. Keyboard only
        /// </summary>
        public string state { get; set; }

        /// <summary>
        /// Type of the event. Example: KeyboardEvents, MouseEvents
        /// </summary>
        public string eventType { get; set; }

        /// <summary>
        /// X Coordinates relative to the entire screen. Mouse only
        /// </summary>
        public string xPosition { get; set; }

        /// <summary>
        /// Y Coordinates relative to the entire screen. Mouse only
        /// </summary>
        public string yPosition { get; set; }

        /// <summary>
        /// Which button was pressed. Mouse only
        /// </summary>
        public string buttonPressed { get; set; }

        /// <summary>
        /// Number of times the button was pressed. Mouse only
        /// </summary>
        public string buttonPressCount { get; set; }

        /// <summary>
        /// Offset of the mouse wheel. Mouse only
        /// </summary>
        public string mouseWheelOffset { get; set; }
    }

    internal struct BatteryInfo {
        /// <summary>
        /// Battery charging status: Charging, Critical, High, Low, NoSystemBattery, Unknown
        /// </summary>
        public string batteryChargeStatus {
            get; set;
        }

        /// <summary>
        /// Full lifetime of the battery in seconds
        /// </summary>
        public string batteryFullLifetime {
            get; set;
        }

        /// <summary>
        /// Percentage of the remaining battery life
        /// </summary>
        public string batteryLifePercent {
            get; set;
        }

        /// <summary>
        /// Remaining battery lifetime in seconds
        /// </summary>
        public string batteryLifeRemaining {
            get; set;
        }

        /// <summary>
        /// Power line status: Offline, Online, Unknown
        /// </summary>
        public string powerLineStatus {
            get; set;
        }
    }
}
