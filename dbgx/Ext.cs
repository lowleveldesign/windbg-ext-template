namespace DbgXExt.Lib;

using WindowsDebugger.DbgEng;

public sealed class Ext
{
    public delegate HRESULT ExtEntry(IDebugClient client, string s);

    public static HRESULT HelloWorld(IDebugClient client, string args) {
        var ctrl = (IDebugControl7)client;
        ctrl.OutputWide(DEBUG_OUTPUT.NORMAL, "Hello world!\n");
        return HRESULT.S_OK;
    }

    public static HRESULT HelloWorld2(IDebugClient client, string args) {
        var ctrl = (IDebugControl7)client;
        ctrl.OutputWide(DEBUG_OUTPUT.NORMAL, "Hello world again!\n");
        return HRESULT.S_OK;
    }
}
