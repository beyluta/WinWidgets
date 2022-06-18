using System;
using System.Linq;
using Widgets;
using Widgets.Manager;

internal class Program : WindowEssentials
{
    [STAThread]
    private static void Main(string[] args)
    {
        ShowWindow(GetConsoleWindow(), SW_HIDE);

        var exists = System.Diagnostics.Process.GetProcessesByName(System.IO.Path.GetFileNameWithoutExtension(System.Reflection.Assembly.GetEntryAssembly().Location)).Count() > 1;
        if (!exists)
        {
            WidgetsManager manager = new WidgetsManager();
        }
    }
}