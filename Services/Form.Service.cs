using System.Windows.Forms;

namespace Services
{
    internal class FormService
    {
        /// <summary>
        /// Sets the Form's window opacity
        /// </summary>
        /// <param name="window">The instance of the form</param>
        /// <param name="opacity">The amount of opacity</param>
        public void SetWindowOpacity(Form window, int opacity)
        {
            window.Opacity = opacity;
        }

        /// <summary>
        /// Sets the state of the Form's window
        /// </summary>
        /// <param name="window">The instance of the form</param>
        /// <param name="state">The state of the window</param>
        public void SetWindowState(Form window, FormWindowState state)
        {
            window.WindowState = state;
        }

        /// <summary>
        /// Checks if the Form's window state is desired
        /// </summary>
        /// <param name="window">The instance of the form</param>
        /// <param name="state">The desired state of the window</param>
        /// <returns>Whether the desired state is set</returns>
        public bool FormStateAssert(Form window, FormWindowState state)
        {
            if (window.WindowState == state)
            {
                return true;
            }

            return false;
        }

        /// <summary>
        /// Brings the Form's window back to the screen if hidden
        /// </summary>
        /// <param name="window">The instance of the form</param>
        public void WakeWindow(Form window)
        {
            SetWindowOpacity(window, 100);
            SetWindowState(window, FormWindowState.Normal);
        }
    }
}
