using System.Linq;
using System.Threading;
using Widgets;
using Widgets.Manager;

internal class Program : WindowEssentials
{
    private static void Main(string[] args)
    {
        ShowWindow(GetConsoleWindow(), SW_HIDE);

        var exists = System.Diagnostics.Process.GetProcessesByName(System.IO.Path.GetFileNameWithoutExtension(System.Reflection.Assembly.GetEntryAssembly().Location)).Count() > 1;
        if (!exists)
        {
            Thread thread = new Thread(() =>
            {
                WidgetsManager manager = new WidgetsManager();
            });
            thread.SetApartmentState(ApartmentState.STA);
            thread.Start();
        }
    }
}