# Managed WinDbg extension template

This repository contains code for a simple WinDbg extension. It implements two extension methods (`hello_world`, `hello_world2`) to prove that the idea works.

## How it works

The [ext.cpp](blob/main/ext.cpp) file contains the C++ code to load .NET runtime into the WinDbg process. The code is based on [an excellent sample](https://github.com/dotnet/samples/tree/main/core/hosting) from the [dotnet samples repository](https://github.com/dotnet/samples). Each extension method that WinDbg accepts must have the following signature:

```cpp
HRESULT ext_method(IDebugClient* client, LPCSTR args)
```

`IDebugClient` is one of the [many COM interfaces](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dbgeng/) we use to communicate with the debug engine (dbgeng). In the past, accessing those interfaces from the managed world was complicated. Some of them were available in the [clrmd](https://github.com/microsoft/clrmd/tree/main/src/Microsoft.Diagnostics.Runtime/src/DbgEng) project, but in a hard-to-consume form. Thanks to the [Microsoft.Debugging.Platform.DbgX nuget package](https://www.nuget.org/packages/Microsoft.Debugging.Platform.DbgX), we can now access them as any other COM objects. The Runtime-callable wrappers perform all the necessary actions to wrap and query the native COM objects. For example, to print something on the WinDbg output, we may cast the `IDebugClient` instance to `IDebugControl7` and call the `OutputWide` method:

```csharp
public static HRESULT HelloWorld(IDebugClient client, string args) {
        var ctrl = (IDebugControl7)client;
        ctrl.OutputWide(DEBUG_OUTPUT.NORMAL, "Hello world!\n");
}
```

From now, our options are endless. Check the [available interfaces](https://www.fuget.org/packages/Microsoft.Debugging.Platform.DbgX/latest/lib/net6.0-windows10.0.17763/DbgX.dll/WindowsDebugger.DbgEng) and [dbgeng help](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dbgeng/) to learn more.

## Building

Make sure you have C++ tools and Windows SDK installed. Then call: `dotnet build`.

Although the managed part could be compiled for any CPU, the native bootstrapper is either 64 or 32-bit. To explicitly set the bitness, specify the runtime identifier in the build command, for example, `dotnet build --runtime=win-x86 --no-self-contained` to build a 32-bit extension.

As a result of the build, you should find **dbgxext.dll** and **DbgXExt.Lib.dll** libraries in the output folder, among many other dependencies. The next step is to load **dbgext.dll** into WinDbg and execute the extension commands:

```
0:001> .load c:\Users\me\repos\windbg-ext-template\bin\Debug\net6.0-windows10.0.17763\win-x64\dbgxext.dll
0:001> !hello_world
Hello world!
0:001> !hello_world2
Hello world again!
```

## Customizing the template for your own purpose

First, naming :) To change the manage library name, rename the csproj file. Then, you will need to accordingly update the constant strings in the **ext.cpp** file:

```cpp
const wchar_t* dotnet_lib_name = L"DbgXExt.Lib.dll";
const wchar_t* dotnet_lib_config = L"DbgXExt.Lib.runtimeconfig.json";
const wchar_t* dotnet_type = L"DbgXExt.Lib.Ext, DbgXExt.Lib";
const wchar_t* dotnet_delegate_type = L"DbgXExt.Lib.Ext+ExtEntry, DbgXExt.Lib";
```

To change the name of the native bootstrapper, modify it in the csproj file:

```xml
<PropertyGroup>
  <NativeOutputName>dbgxext</NativeOutputName>
</PropertyGroup>
```

Finally, adding a new extension method goes in three steps:

1. Add a new method to the managed **Ext** class, for example:

```csharp
public sealed class Ext
{
    ...

    public static HRESULT MyAwesomeExtensionMethod(IDebugClient client, string args) {
        ...
    }
}
```

2. Add a new method to the native **ext.cpp** file, for example:

```cpp
extern "C" HRESULT CALLBACK my_awesome_extension_method(IDebugClient * dbgclient, PCSTR args) {
    static managed_extension_method fn{};
    static std::once_flag called{};
    std::call_once(called, []() {
        const std::wstring dotnetlib_path{ basedir / dotnet_lib_name };
        if (auto hr = loading_fn(dotnetlib_path.c_str(), dotnet_type, L"MyAwesomeExtensionMethod",
            dotnet_delegate_type, nullptr, reinterpret_cast<void**>(&fn)); FAILED(hr)) {
            fn = nullptr;
        }
        });

    return fn ? fn(dbgclient, args) : E_FAIL;
}
```

3. Export the native method by adding it to the **ext.def** file, for example:

```
EXPORTS
    DebugExtensionNotify
    DebugExtensionInitialize
    DebugExtensionUninitialize

    my_awesome_extension_method
```

Then, after loading the bootstrapper dll, launch your method with the `!my_awesome_extension_method` command.

## Things to improve

The extension requires only a small subset of the DbgX NuGet package. Unfortunately, the package source code is unavailable so we need to import all the dependencies, unnecessarily consuming disk space. I hope to improve that one day..

Adding a new method is a copy-paste exercise. I feel that it could be done better with some code-generation technique :)

## Alternative approches

If you don't want to mess with C++ code, check the [CLRMDExports](https://github.com/kevingosse/ClrMDExports) project by Kevin Gosse. It is based on the clrmd package and could be a better option if you're implementing an extension to debug managed processes. Kevin describe this approach in [an article on Medium](https://medium.com/@kevingosse/writing-native-windbg-extensions-in-c-5390726f3cec).
