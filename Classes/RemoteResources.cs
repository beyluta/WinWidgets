using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using Widgets.Manager;

namespace Remote
{
    public class RemoteResources
    {
        private Configuration appConfig;

        public RemoteResources()
        {
            string json = File.ReadAllText(WidgetAssets.assetsPath + "/config.json");
            appConfig = JsonConvert.DeserializeObject<Configuration>(json);
        }

        public async void DownloadRemoteResources()
        {
            using (HttpClient client = new HttpClient())
            {
                try
                {
                    string response = await client.GetStringAsync(appConfig.remoteResources);
                    JArray array = JArray.Parse(response);
                    foreach (JObject resource in array)
                    {
                        string name = (string)resource["name"];
                        string content = (string)resource["html"];
                        string path = WidgetAssets.widgetsPath + $"\\{name}.html";
                        WidgetAssets.CreateHTMLFile(path, content);
                    }
                }
                catch { }
            }
        }
    }
}
