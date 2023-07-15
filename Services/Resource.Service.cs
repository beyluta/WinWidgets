using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System.IO;
using System.Net.Http;
using System.Threading.Tasks;

namespace Services
{
    public class ResourceService
    {
        private ConfigurationModel appConfig;

        public ResourceService()
        {
            string json = File.ReadAllText(AssetService.assetsPath + "/config.json");
            appConfig = JsonConvert.DeserializeObject<ConfigurationModel>(json);
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
                        string path = AssetService.widgetsPath + $"\\{name}.html";
                        AssetService.CreateHTMLFile(path, content);
                    }
                }
                catch { }
            }
        }

        public async Task<JObject> GetRemoteVersion()
        {
            JObject jObject;

            using (HttpClient client = new HttpClient())
            {
                jObject = JObject.Parse("{\"version\":\"" + appConfig.version + "\",\"downloadUrl\":\"https://github.com/beyluta/WinWidgets\"}");

                try
                {
                    string response = await client.GetStringAsync("https://7xdeveloper.com/api/AccessEndpoint.php?endpoint=getappconfigs&id=version");
                    jObject = JObject.Parse(response);
                }
                catch { }
            }

            return jObject;
        }
    }
}
