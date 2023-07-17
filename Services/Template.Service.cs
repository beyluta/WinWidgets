using Services;

namespace Service
{
    internal class TemplateService
    {
        /// <summary>
        /// Gets all templates from the template folder and loads them as widgets
        /// </summary>
        /// <returns>An array of absolute path to every template file</returns>
        public string[] GetAllTemplates()
        {
            return AssetService.GetPathToHTMLFiles(AssetService.templatePath);
        }
    }
}
