// SPDX-License-Identifier: MIT
// Portable production-helper byte/ABI controls; never Windows registry/GPU proof.
using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

static class SystemD3D10BindingControls {
    static int checks;
    static void Check(bool value) { ++checks; if (!value) throw new Exception("Control "+checks+" failed"); }
    static void Reject(Action action) {
        bool rejected=false;
        try { action(); } catch (ArgumentException) { rejected=true; } catch (InvalidOperationException) { rejected=true; }
        Check(rejected);
    }
    static DxvkBindingRawValue01 Value(uint view,string name,bool exists,uint type,byte[] bytes) {
        return new DxvkBindingRawValue01 { View=view,Name=name,Exists=exists,Type=type,Data=bytes };
    }
    static DxvkBindingRawValue01 Multi(string text) { return Value(0x100,"UserModeDriverName",true,7,Encoding.Unicode.GetBytes(text)); }
    static bool Equal(byte[] first,byte[] second) {
        if (first.Length != second.Length) return false;
        for (int i=0;i<first.Length;++i) if (first[i] != second[i]) return false;
        return true;
    }
    static DxvkBindingNames01 Names(string path) {
        var rows=new DxvkBindingName01[3];
        for (int i=0;i<3;++i) rows[i]=new DxvkBindingName01 { Version=(uint)i,Status=0,Name=path,Raw=new byte[524] };
        return new DxvkBindingNames01 { Luid="00000000:0000abcd",OpenStatus=0,CloseStatus=0,Names=rows };
    }
    static int Main() {
        const string first=@"C:\Windows\DriverStore\Mesa9.dll", old=@"C:\Windows\DriverStore\Mesa10.dll", last=@"C:\Windows\DriverStore\Mesa11.dll";
        string[] candidates={@"C:\V\f.dll",@"C:\ReviewedValidation\viogpudxvk_validate10.dll",@"D:\長い検証ディレクトリ\driver.dll"};
        for (int padding=2;padding<=7;++padding) foreach (string candidate in candidates) {
            string suffix=new string('\0',padding);
            var original=Multi(first+"\0"+old+"\0"+last+suffix);
            var before=(byte[])original.Data.Clone();
            var actual=DxvkBindingNative01.ReplaceDx10(original,candidate);
            Check(Equal(actual.Data,Encoding.Unicode.GetBytes(first+"\0"+candidate+"\0"+last+suffix)));
            Check(Equal(original.Data,before));
            Check(actual.View == 0x100 && actual.Name == "UserModeDriverName" && actual.Exists && actual.Type == 7);
            string[] slots=DxvkBindingNative01.Tuple(actual);
            Check(slots.Length == 3 && slots[0] == first && slots[1] == candidate && slots[2] == last);
        }
        foreach (string invalid in new string[] { null,"","front.dll",@"\Windows\front.dll",@"C:front.dll",@"C:\V\..\front.dll",@"C:\V\.\front.dll",@"C:\\front.dll",@"C:\V\front?.dll","C:\\V\\fr\0ont.dll","C:\\V\\fr\"ont.dll","C:\\V\\\ud800.dll" })
            Reject(delegate { DxvkBindingNative01.ReplaceDx10(Multi(first+"\0"+old+"\0"+last+"\0\0"),invalid); });
        foreach (string invalid in new string[] { "",first+"\0"+old+"\0",first+"\0"+old+"\0"+last+"\0",first+"\0\0"+last+"\0\0",first+"\0"+old+"\0"+last+"\0C:\\four.dll\0\0","relative\0"+old+"\0"+last+"\0\0" })
            Reject(delegate { DxvkBindingNative01.ReplaceDx10(Multi(invalid),candidates[0]); });
        var odd=Multi(first+"\0"+old+"\0"+last+"\0\0"); Array.Resize(ref odd.Data,odd.Data.Length-1);
        Reject(delegate { DxvkBindingNative01.ReplaceDx10(odd,candidates[0]); });
        Reject(delegate { DxvkBindingNative01.ReplaceDx10(Value(0x100,"UserModeDriverName",false,0,new byte[0]),candidates[0]); });
        Reject(delegate { DxvkBindingNative01.ReplaceDx10(Value(0x100,"UserModeDriverName",true,1,new byte[] {0,0}),candidates[0]); });
        var rows=new DxvkBindingRawValue01[6];
        string[] names={"UserModeDriverName","UserModeDriverNameWoW","InstalledDisplayDrivers"};
        for (int i=0;i<6;++i) rows[i]=Value(i<3 ? 0x100u : 0x200u,names[i%3],i%3 != 1,i%3 == 1 ? 0u : 7u,i%3 == 1 ? new byte[0] : Encoding.Unicode.GetBytes(first+"\0\0"));
        DxvkBindingNative01.RequireSnapshot(rows,rows); Check(true);
        for (int i=0;i<6;++i) {
            var copy=(DxvkBindingRawValue01[])rows.Clone();
            copy[i]=Value(rows[i].View,rows[i].Name,rows[i].Exists,rows[i].Type,(byte[])rows[i].Data.Clone());
            Check(DxvkBindingNative01.Same(rows[i],copy[i]));
            copy[i].Type ^= 1;
            Reject(delegate { DxvkBindingNative01.RequireSnapshot(rows,copy); });
        }
        foreach (int length in new int[] {0,1,5,7}) Reject(delegate { DxvkBindingNative01.RequireSnapshot(rows,new DxvkBindingRawValue01[length]); });
        var baseNames=Names(first); Check(DxvkBindingNative01.NamesEqual(baseNames,Names(first.ToUpperInvariant())));
        Check(!DxvkBindingNative01.NamesEqual(baseNames,Names(last)));
        var changed=Names(first); changed.Luid="00000000:0000abce"; Check(!DxvkBindingNative01.NamesEqual(baseNames,changed));
        changed=Names(first); changed.CloseStatus=-1; Check(!DxvkBindingNative01.NamesEqual(baseNames,changed));
        for (int i=0;i<3;++i) { changed=Names(first); changed.Names[i].Status=-1; Check(!DxvkBindingNative01.NamesEqual(baseNames,changed)); }
        foreach (var expected in new object[][] {new object[] {"OpenLuid",12},new object[] {"Query",24},new object[] {"JobLimits",144}}) {
            var type=typeof(DxvkBindingNative01).GetNestedType((string)expected[0],BindingFlags.NonPublic);
            Check(type != null && Marshal.SizeOf(type) == (int)expected[1]);
        }
        Console.WriteLine("D10 binding helper controls PASS checks="+checks+" windows_registry=0 windows_process=0 hardware_admission=0");
        return 0;
    }
}
