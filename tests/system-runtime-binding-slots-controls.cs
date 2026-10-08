// SPDX-License-Identifier: MIT
// Independent literal registration bytes and actual probe-machine boundary.
using System;
using System.Text;
using System.IO;
static class SystemRuntimeBindingSlotsControls {
    static int checks;
    public static int CheckCount { get { return checks; } }
    static void Check(bool passed) { ++checks; if (!passed) throw new Exception("Slot control "+checks+" failed"); }
    static void Reject(Action action) { bool failed=false; try { action(); } catch(ArgumentException) { failed=true; }; Check(failed); }
    static bool Equal(byte[] a,byte[] b) { if(a.Length!=b.Length)return false; for(int i=0;i<a.Length;++i)if(a[i]!=b[i])return false; return true; }
    static DxvkBindingRawValue01 Row(string value) { return new DxvkBindingRawValue01 { View=0x100,Name="UserModeDriverName",Exists=true,Type=7,Data=Encoding.Unicode.GetBytes(value) }; }
    static byte[] Pe(uint machine) { var b=new byte[134]; b[0]=0x4d;b[1]=0x5a;b[60]=128;b[128]=0x50;b[129]=0x45;b[132]=(byte)machine;b[133]=(byte)(machine>>8);return b; }
    static int Main() {
        string d9=@"C:\Original\mesa9.dll",d10=@"C:\Original\mesa10.dll",d11=@"C:\Original\mesa11.dll";
        foreach(string candidate in new[]{@"C:\V\f.dll",@"C:\Reviewed\arm64\long-candidate.dll",@"C:\測\試\front.dll"})
        foreach(string suffix in new[]{"\0\0","\0\0\0","\0\0\0\0\0\0"})
        foreach(int slot in new[]{0,1,2}) {
            var original=Row(d9+"\0"+d10+"\0"+d11+suffix); var before=(byte[])original.Data.Clone();
            var actual=DxvkBindingNative01.ReplaceNativeSlot(original,candidate,slot);
            string expected=slot==0 ? candidate+"\0"+d10+"\0"+d11+suffix : slot==1 ? d9+"\0"+candidate+"\0"+d11+suffix : d9+"\0"+d10+"\0"+candidate+suffix;
            Check(Equal(actual.Data,Encoding.Unicode.GetBytes(expected))); Check(Equal(original.Data,before));
            Check(actual.View==0x100 && actual.Name=="UserModeDriverName" && actual.Type==7 && actual.Exists);
            var names=DxvkBindingNative01.Tuple(actual); Check(names[0]==(slot==0 ? candidate : d9) && names[1]==(slot==1 ? candidate : d10) && names[2]==(slot==2 ? candidate : d11));
            if(slot==1) Check(Equal(actual.Data,DxvkBindingNative01.ReplaceDx10(original,candidate).Data));
        }
        foreach(int invalid in new[]{-1,3,4,Int32.MaxValue}) Reject(delegate { DxvkBindingNative01.ReplaceNativeSlot(Row(d9+"\0"+d10+"\0"+d11+"\0\0"),@"C:\V\f.dll",invalid); });
        var wow=Row(d9+"\0"+d10+"\0"+d11+"\0\0");wow.View=0x200;Reject(delegate { DxvkBindingNative01.ReplaceNativeSlot(wow,@"C:\V\f.dll",0); });
        var other=Row(d9+"\0"+d10+"\0"+d11+"\0\0");other.Name="InstalledDisplayDrivers";Reject(delegate { DxvkBindingNative01.ReplaceNativeSlot(other,@"C:\V\f.dll",0); });
        Check(DxvkBindingNative01.ProbeArchitecture(Pe(0xaa64))=="arm64");Check(DxvkBindingNative01.ProbeArchitecture(Pe(0x8664))=="x64");
        foreach(uint invalid in new uint[]{0,0x14c,0xa641,0xffff}) Reject(delegate { DxvkBindingNative01.ProbeArchitecture(Pe(invalid)); });
        foreach(int length in new[]{0,63,64,128,133}) Reject(delegate { var b=Pe(0xaa64);Array.Resize(ref b,length);DxvkBindingNative01.ProbeArchitecture(b); });
        var missing=Pe(0xaa64);missing[0]=0;Reject(delegate { DxvkBindingNative01.ProbeArchitecture(missing); });
        missing=Pe(0xaa64);missing[130]=1;Reject(delegate { DxvkBindingNative01.ProbeArchitecture(missing); });
        missing=Pe(0xaa64);missing[63]=0xff;Reject(delegate { DxvkBindingNative01.ProbeArchitecture(missing); });
        string checkpoint=Path.Combine(Environment.CurrentDirectory,"binding-closed-"+Guid.NewGuid().ToString("N")+".json");
        try {
            Check(DxvkBindingNative01.ReadClosedText(checkpoint)==null);
            const string literal="{\"pid\":123,\"hold_event\":\"Local\\\\測試\"}";
            byte[] bytes=new UTF8Encoding(false).GetBytes(literal);
            using(var writer=new FileStream(checkpoint,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                writer.Write(bytes,0,bytes.Length);writer.Flush(true);
                Check(DxvkBindingNative01.ReadClosedText(checkpoint)==null);
            }
            Check(DxvkBindingNative01.ReadClosedText(checkpoint)==literal);
            File.Delete(checkpoint);Check(DxvkBindingNative01.ReadClosedText(checkpoint)==null);
        } finally { if(File.Exists(checkpoint))File.Delete(checkpoint); }
        Console.WriteLine("SYSTEM binding slots controls PASS checks="+checks+" native_slots=3 windows_registry=0 windows_process=0 hardware_admission=0");return 0;
    }
}
