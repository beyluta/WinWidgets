namespace Models
{
    internal enum HardwareEvent
    {
        NativeKeys = 0,
        BatteryLevel = 1,
        SpaceAvailable = 2,
        DeviceTemperature = 3
    }

    internal enum HardwareMetrics
    {
        Fahrenheit = 0,
        Celcius = 1
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
}
