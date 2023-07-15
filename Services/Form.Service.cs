using System.Windows.Forms;

namespace Services
{
    internal class FormService
    {
        public void SetWindowOpacity(Form window, int opacity)
        {
            window.Opacity = opacity;
        }

        public void SetWindowState(Form window, FormWindowState state)
        {
            window.WindowState = state;
        }

        public bool FormStateAssert(Form window, FormWindowState state)
        {
            if (window.WindowState == state)
            {
                return true;
            }

            return false;
        }

        public void WakeWindow(Form window)
        {
            SetWindowOpacity(window, 100);
            SetWindowState(window, FormWindowState.Normal);
        }
    }
}
