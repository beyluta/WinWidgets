using Models;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Windows.Forms;

namespace Services
{
    static class AssetService
    {
        /// <summary>
        /// Global service where all widgets are stored
        /// </summary>
        static public WidgetListService widgets = new WidgetListService();

        /// <summary>
        /// Path where the assets of the application are stored
        /// </summary>
        static public string assetsPath = Application.StartupPath + "/Assets";

        /// <summary>
        /// Path to the Widget templates
        /// </summary>
        static public string templatePath = Path.Combine(assetsPath) + "/Templates";

        /// <summary>
        /// Path to the html widgets
        /// </summary>
        static public string widgetsPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Widgets");

        /// <summary>
        /// Path to the config path
        /// </summary>
        static public string configPath = Path.Combine(widgetsPath) + "/config.json";

        /// <summary>
        /// Semantic version of the application
        /// </summary>
        static private string version = "1.3.2";

        /// <summary>
        /// Gets the path where the HTML files (widgets) of the project are stored
        /// </summary>
        /// <param name="path">Path to the root folder</param>
        /// <returns>An array of all html files and their absolute paths</returns>
        static public string[] GetPathToHTMLFiles(string path)
        {
            return Directory.GetFiles(path, "*.html", SearchOption.AllDirectories);
        }

        /// <summary>
        /// Creates the default directory for all html files (widgets)
        /// </summary>
        static public void CreateHTMLFilesDirectory()
        {
            Directory.CreateDirectory(widgetsPath);
        }

        /// <summary>
        /// Creates a html file if it doesn't exist
        /// </summary>
        /// <param name="path">Path of creation and name of the file</param>
        /// <param name="content">Content of the folder</param>
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

        /// <summary>
        /// Moves all files from the source path to the destination path
        /// </summary>
        /// <param name="source">Source path where the files are</param>
        /// <param name="destination">Destination path where the files must go</param>
        static public void MoveFilesToPath(string source, string destination)
        {
            if (Directory.Exists(destination))
            {
                string[] files = GetPathToHTMLFiles(source);

                foreach (string path in files)
                {
                    string destinationFile = Path.Combine(destination, Path.GetFileName(path));

                    if (!File.Exists(destinationFile))
                    {
                        File.Move(path, destinationFile);
                    }
                }
            }
        }

        /// <summary>
        /// Overwrites the configuration file with new content
        /// </summary>
        /// <param name="content">Configuration object to be overwritten</param>
        static public void OverwriteConfigurationFile(Configuration configuration)
        {
            if (configuration.version != version)
            {
                configuration.version = version;
            }

            File.WriteAllText(configPath, JsonConvert.SerializeObject(configuration));
        }

        /// <summary>
        /// Get all configuration from the configuration file
        /// </summary>
        /// <returns>Configuration struct with all configurations</returns>
        static public Configuration GetConfigurationFile()
        {
            if (!File.Exists(AssetService.configPath))
            {
                OverwriteConfigurationFile(new Configuration() { 
                    isWidgetAutostartEnabled = false, 
                    lastSessionWidgets = new List<WidgetConfiguration>(),
                    isWidgetFullscreenHideEnabled = false,
                    hideWidgetManagerOnStartup = false,
                    version = version
                });

                return GetConfigurationFile();
            }

            try
            {
                return JsonConvert.DeserializeObject<Configuration>(File.ReadAllText(AssetService.configPath));
            } catch
            {
                File.Delete(AssetService.configPath);
                return GetConfigurationFile();
            }
        }
    }
}
