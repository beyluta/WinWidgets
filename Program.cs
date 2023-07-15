﻿using Controllers;
using Models;
using Services;
using System;
using System.Linq;

internal class Program : WindowModel
{
    [STAThread]
    private static void Main(string[] args)
    {
        bool processExists = System.Diagnostics.Process.GetProcessesByName(
            System.IO.Path.GetFileNameWithoutExtension(
                System.Reflection.Assembly.GetEntryAssembly().Location))
                    .Count() > 1;

        if (!processExists)
        {
            new WidgetsManagerController(new WidgetService(), new ResourceService());
        }
    }
}