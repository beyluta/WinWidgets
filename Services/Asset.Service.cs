using Controllers;
using System;
using System.IO;
using System.Windows.Forms;

namespace Services
{
    static class AssetService
    {
        static public WidgetListService widgets = new WidgetListService();
        static public string assetsPath = Application.StartupPath + "/Assets";
        static public string widgetsPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Widgets");

        static public string[] GetPathToHTMLFiles(string path)
        {
            return Directory.GetFiles(path, "*.html", SearchOption.AllDirectories);
        }

        static public void CreateHTMLFilesDirectory()
        {
            Directory.CreateDirectory(widgetsPath);
        }

        static public void CreateHTMLFile(string path, string content)
        {
            if (!File.Exists(path))
            {
                using (StreamWriter streamWriter = new StreamWriter(path, true))
                {
                    streamWriter.WriteLine(content);
                }
            }
        }

        static public void RemoveWidget(WidgetController widget)
        {
           widgets.RemoveWidget(widget);
        }
    }
}
