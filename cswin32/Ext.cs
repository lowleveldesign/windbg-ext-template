namespace DbgXExt.Lib;

using Windows.Win32.System.Diagnostics.Debug;
using Windows.Win32.Foundation;

public sealed class Ext
{
    public delegate HRESULT ExtEntry(IDebugClient client, string s);

    public static unsafe HRESULT HelloWorld(IDebugClient client, string args)
    {
        var ctrl = (IDebugControl7)client;
        fixed (char* s = "Hello world!\n")
        {
            ctrl.OutputWide(1, s);
        }
        return HRESULT.S_OK;
    }

    public static unsafe HRESULT HelloWorld2(IDebugClient client, string args)
    {
        var ctrl = (IDebugControl7)client;
        fixed (char* s = "Hello world again!\n")
        {
            ctrl.OutputWide(1, s);
        }
        return HRESULT.S_OK;
    }
}

