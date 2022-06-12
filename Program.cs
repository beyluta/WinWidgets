using System.Threading;
using Widgets;
using Widgets.Manager;
using System;
using Newtonsoft.Json;
using System.IO;

internal class Program : WindowEssentials
{
    static void Main(string[] args)
    {
        //ShowWindow(GetConsoleWindow(), SW_HIDE);

        Thread thread = new Thread(() =>
        {
            WidgetsManager manager = new WidgetsManager();
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
    }
}

