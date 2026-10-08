// SPDX-License-Identifier: MIT
// Validation-controller support. No production driver/caps or cache overrides.
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public sealed class DxvkBindingRawValue01 {
    public uint View;
    public string Name;
    public bool Exists;
    public uint Type;
    public byte[] Data;
}
public sealed class DxvkBindingName01 {
    public uint Version;
    public int Status;
    public string Name;
    public byte[] Raw;
}
public sealed class DxvkBindingNames01 {
    public string Luid;
    public int OpenStatus;
    public int CloseStatus;
    public string GdiPath;
    public DxvkBindingName01[] Names;
}

public static class DxvkBindingNative01 {
    static readonly string[] ValueNames = { "UserModeDriverName", "UserModeDriverNameWoW", "InstalledDisplayDrivers" };
    static readonly IntPtr Hklm = new IntPtr(unchecked((int)0x80000002));
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegOpenKeyEx(IntPtr root, string key, uint options, uint access, out IntPtr result);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegQueryValueEx(IntPtr key, string name, IntPtr reserved, out uint type, byte[] data, ref uint bytes);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegSetValueEx(IntPtr key, string name, uint reserved, uint type, byte[] data, uint bytes);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern int RegDeleteValue(IntPtr key, string name);
    [DllImport("advapi32.dll")] static extern int RegFlushKey(IntPtr key);
    [DllImport("advapi32.dll")] static extern int RegCloseKey(IntPtr key);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr LoadLibraryEx(string file, IntPtr reserved, uint flags);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern uint GetModuleFileName(IntPtr module, StringBuilder path, uint capacity);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr CreateJobObject(IntPtr attributes, string name);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job, uint infoClass, ref JobLimits limits, uint bytes);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [StructLayout(LayoutKind.Sequential)] struct BasicLimits {
        public long ProcessTime, JobTime;
        public uint Flags;
        public UIntPtr MinWorkingSet, MaxWorkingSet;
        public uint ActiveProcesses;
        public UIntPtr Affinity;
        public uint Priority, Scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct IoCounters { public ulong ReadOperations, WriteOperations, OtherOperations, ReadBytes, WriteBytes, OtherBytes; }
    [StructLayout(LayoutKind.Sequential)] struct JobLimits {
        public BasicLimits Basic;
        public IoCounters Io;
        public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
    }
    static IntPtr workerJob;
    public static long OwnWorkerLifetime() {
        if (IntPtr.Size != 8 || Marshal.SizeOf(typeof(JobLimits)) != 144 || workerJob != IntPtr.Zero)
            throw new InvalidOperationException("One original native worker job required");
        IntPtr job=CreateJobObject(IntPtr.Zero,null);
        if (job == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        var limits=new JobLimits(); limits.Basic.Flags=0x2000; // KILL_ON_JOB_CLOSE
        if (!SetInformationJobObject(job,9,ref limits,144) || !AssignProcessToJobObject(job,GetCurrentProcess())) {
            int error=Marshal.GetLastWin32Error(); CloseHandle(job); throw new Win32Exception(error);
        }
        // Hold this unnamed noninherited job handle through process lifetime.
        // Ordinary Process.Start children remain in the job; worker crash,
        // scheduled-task stop or session loss cannot leave an orphan probe.
        workerJob=job; return job.ToInt64();
    }
    static void Check(int status) { if (status != 0) throw new Win32Exception(status); }
    static IntPtr Open(string subkey, uint view, bool write) {
        if (view != 0x100 && view != 0x200) throw new ArgumentException("Explicit native/WoW registry view required");
        IntPtr key; Check(RegOpenKeyEx(Hklm, subkey, 0, (write ? 3u : 1u) | view, out key)); return key;
    }
    public static DxvkBindingRawValue01 Read(string subkey, uint view, string name) {
        IntPtr key = Open(subkey, view, false);
        try {
            uint bytes = 0, type;
            int status = RegQueryValueEx(key, name, IntPtr.Zero, out type, null, ref bytes);
            if (status == 2) return new DxvkBindingRawValue01 { View=view, Name=name, Exists=false, Data=new byte[0] };
            Check(status);
            for (int attempt=0; attempt<3; ++attempt) {
                if (bytes > 1048576) throw new InvalidOperationException("Registry value exceeds bounded snapshot");
                byte[] data = new byte[bytes];
                uint actual = bytes;
                status = RegQueryValueEx(key, name, IntPtr.Zero, out type, data, ref actual);
                if (status == 234) { bytes=actual; continue; }
                Check(status);
                if (actual != data.Length) Array.Resize(ref data, checked((int)actual));
                return new DxvkBindingRawValue01 { View=view, Name=name, Exists=true, Type=type, Data=data };
            }
            throw new InvalidOperationException("Registry snapshot changed repeatedly");
        } finally { Check(RegCloseKey(key)); }
    }
    public static DxvkBindingRawValue01[] Snapshot(string subkey) {
        var rows = new DxvkBindingRawValue01[6]; int index=0;
        foreach (uint view in new uint[] { 0x100, 0x200 })
            foreach (string name in ValueNames) rows[index++]=Read(subkey, view, name);
        return rows;
    }
    public static bool Same(DxvkBindingRawValue01 first, DxvkBindingRawValue01 second) {
        if (first == null || second == null || first.View != second.View || first.Name != second.Name
            || first.Exists != second.Exists || first.Type != second.Type || first.Data == null || second.Data == null
            || first.Data.Length != second.Data.Length) return false;
        for (int i=0; i<first.Data.Length; ++i) if (first.Data[i] != second.Data[i]) return false;
        return true;
    }
    public static void RequireSnapshot(DxvkBindingRawValue01[] expected, DxvkBindingRawValue01[] actual) {
        if (expected == null || actual == null || expected.Length != 6 || actual.Length != 6)
            throw new InvalidOperationException("Exact six-value tuple snapshot required");
        for (int i=0; i<6; ++i) if (!Same(expected[i], actual[i])) throw new InvalidOperationException("Raw registry tuple differs: "+expected[i].View+"/"+expected[i].Name);
    }
    public static void Write(string subkey, DxvkBindingRawValue01 row) {
        IntPtr key=Open(subkey, row.View, true);
        try {
            if (row.Exists) Check(RegSetValueEx(key, row.Name, 0, row.Type, row.Data, checked((uint)row.Data.Length)));
            else { int status=RegDeleteValue(key, row.Name); if (status != 2) Check(status); }
            Check(RegFlushKey(key));
        } finally { Check(RegCloseKey(key)); }
    }
    public static void Restore(string subkey, DxvkBindingRawValue01[] expected) {
        if (expected == null || expected.Length != 6) throw new InvalidOperationException("Missing original tuple");
        foreach (DxvkBindingRawValue01 row in expected) Write(subkey, row);
        RequireSnapshot(expected, Snapshot(subkey));
    }
    static string[] ParseTuple(DxvkBindingRawValue01 row) {
        if (row == null || !row.Exists || row.Type != 7 || row.Data == null || row.Data.Length < 8 || row.Data.Length % 2 != 0)
            throw new ArgumentException("Expected original REG_MULTI_SZ bytes");
        string text = new UnicodeEncoding(false, false, true).GetString(row.Data);
        if (!text.EndsWith("\0\0", StringComparison.Ordinal)) throw new ArgumentException("Missing double terminator");
        string[] paths=text.TrimEnd('\0').Split('\0');
        if (paths.Length != 3) throw new ArgumentException("Exactly three D9/D10/D11 slots required");
        foreach (string path in paths) RequireAbsolute(path);
        return paths;
    }
    public static string[] Tuple(DxvkBindingRawValue01 row) { return ParseTuple(row); }
    public static void RequireAbsolute(string value) {
        if (String.IsNullOrEmpty(value) || value.Length < 4 || !((value[0]>='A' && value[0]<='Z') || (value[0]>='a' && value[0]<='z')) || value[1] != ':' || value[2] != '\\'
            || value.IndexOf('\0') >= 0 || value.IndexOf('"') >= 0 || value.IndexOf('/') >= 0)
            throw new ArgumentException("Explicit normalized absolute drive path required");
        string[] parts=value.Substring(3).Split('\\');
        foreach (string part in parts) if (part.Length == 0 || part == "." || part == ".." || part.IndexOfAny(new char[] { ':', '*', '?', '<', '>', '|' }) >= 0)
            throw new ArgumentException("Ambiguous Windows path component");
    }
    public static DxvkBindingRawValue01 ReplaceDx10(DxvkBindingRawValue01 original, string candidate) {
        return ReplaceNativeSlot(original,candidate,1);
    }
    public static DxvkBindingRawValue01 ReplaceNativeSlot(DxvkBindingRawValue01 original, string candidate, int nativeSlot) {
        if (nativeSlot < 0 || nativeSlot > 2 || original == null || original.View != 0x100 || original.Name != "UserModeDriverName")
            throw new ArgumentException("Only native DX9/D10/D11 registration slots are supported");
        string[] paths=ParseTuple(original); RequireAbsolute(candidate);
        byte[] insert = new UnicodeEncoding(false, false, true).GetBytes(candidate);
        int start=0;
        for(int index=0;index<nativeSlot;++index) start=checked(start+(paths[index].Length+1)*2);
        int end=checked(start+paths[nativeSlot].Length*2);
        byte[] bytes=new byte[checked(original.Data.Length-(end-start)+insert.Length)];
        Array.Copy(original.Data, 0, bytes, 0, start);
        Array.Copy(insert, 0, bytes, start, insert.Length);
        Array.Copy(original.Data, end, bytes, start+insert.Length, original.Data.Length-end);
        return new DxvkBindingRawValue01 { View=original.View, Name=original.Name, Exists=true, Type=7, Data=bytes };
    }
    public static string ProbeArchitecture(byte[] bytes) {
        if (bytes == null || bytes.Length < 64 || bytes[0] != 0x4d || bytes[1] != 0x5a)
            throw new ArgumentException("Actual probe PE header required");
        uint offset=(uint)(bytes[60] | bytes[61]<<8 | bytes[62]<<16 | bytes[63]<<24);
        if (offset < 64 || offset > bytes.Length-6 || bytes[offset] != 0x50 || bytes[offset+1] != 0x45 || bytes[offset+2] != 0 || bytes[offset+3] != 0)
            throw new ArgumentException("Bounded actual PE signature required");
        uint machine=(uint)(bytes[offset+4] | bytes[offset+5]<<8);
        if (machine == 0xaa64) return "arm64";
        if (machine == 0x8664) return "x64";
        throw new ArgumentException("Only native ARM64/x64 ordinary validation probes are supported");
    }

    [StructLayout(LayoutKind.Sequential)] struct Luid { public uint Low; public int High; }
    [StructLayout(LayoutKind.Sequential)] struct OpenLuid { public Luid Luid; public uint Adapter; }
    [StructLayout(LayoutKind.Sequential)] struct Query { public uint Adapter; public uint Type; public IntPtr Data; public uint Bytes; }
    [StructLayout(LayoutKind.Sequential)] struct Close { public uint Adapter; }
    [DllImport("gdi32.dll")] static extern int D3DKMTOpenAdapterFromLuid(ref OpenLuid args);
    [DllImport("gdi32.dll")] static extern int D3DKMTQueryAdapterInfo(ref Query args);
    [DllImport("gdi32.dll")] static extern int D3DKMTCloseAdapter(ref Close args);
    public static DxvkBindingNames01 Names(string luid) {
        if (luid == null || luid.Length != 17 || luid[8] != ':') throw new ArgumentException("Exact LUID required");
        uint high=UInt32.Parse(luid.Substring(0,8), System.Globalization.NumberStyles.HexNumber), low=UInt32.Parse(luid.Substring(9,8), System.Globalization.NumberStyles.HexNumber);
        if ((high | low) == 0 || IntPtr.Size != 8 || Marshal.SizeOf(typeof(OpenLuid)) != 12 || Marshal.SizeOf(typeof(Query)) != 24)
            throw new InvalidOperationException("Native ARM64/x64 KMT query ABI required");
        string gdi=Path.Combine(Environment.SystemDirectory, "gdi32.dll");
        IntPtr module=LoadLibraryEx(gdi, IntPtr.Zero, 0x800);
        if (module == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        // Hold genuine SYSTEM GDI through process lifetime, matching existing helpers.
        var actual=new StringBuilder(32768); uint length=GetModuleFileName(module, actual, (uint)actual.Capacity);
        if (length == 0 || length >= actual.Capacity || !String.Equals(gdi, actual.ToString(), StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Actual SYSTEM GDI module path differs");
        var result=new DxvkBindingNames01 { Luid=high.ToString("x8")+":"+low.ToString("x8"), GdiPath=actual.ToString() };
        var opened=new OpenLuid { Luid=new Luid { Low=low, High=unchecked((int)high) } };
        result.OpenStatus=D3DKMTOpenAdapterFromLuid(ref opened);
        if (result.OpenStatus != 0) return result;
        try {
            result.Names=new DxvkBindingName01[3];
            for (uint version=0; version<3; ++version) {
                IntPtr memory=Marshal.AllocHGlobal(524);
                try {
                    Marshal.Copy(new byte[524], 0, memory, 524); Marshal.WriteInt32(memory, checked((int)version));
                    var query=new Query { Adapter=opened.Adapter, Type=1, Data=memory, Bytes=524 };
                    int status=D3DKMTQueryAdapterInfo(ref query); var raw=new byte[524]; Marshal.Copy(memory,raw,0,raw.Length);
                    string text=Encoding.Unicode.GetString(raw,4,520); int end=text.IndexOf('\0');
                    if (status == 0 && (end <= 0 || end >= 260)) throw new InvalidOperationException("Unbounded/empty original UMD filename");
                    result.Names[version]=new DxvkBindingName01 { Version=version, Status=status, Raw=raw, Name=end < 0 ? text : text.Substring(0,end) };
                } finally { Marshal.FreeHGlobal(memory); }
            }
        } finally { var closed=new Close { Adapter=opened.Adapter }; result.CloseStatus=D3DKMTCloseAdapter(ref closed); }
        return result;
    }
    public static bool NamesEqual(DxvkBindingNames01 first, DxvkBindingNames01 second) {
        if (first == null || second == null || first.Luid != second.Luid || first.OpenStatus != 0 || second.OpenStatus != 0
            || first.CloseStatus != 0 || second.CloseStatus != 0 || first.Names == null || second.Names == null
            || first.Names.Length != 3 || second.Names.Length != 3) return false;
        for (int i=0; i<3; ++i) if (first.Names[i].Version != second.Names[i].Version || first.Names[i].Status != 0 || second.Names[i].Status != 0
            || !String.Equals(first.Names[i].Name, second.Names[i].Name, StringComparison.OrdinalIgnoreCase)) return false;
        return true;
    }
    public static string ReadSharedText(string path) {
        if (!File.Exists(path)) return "";
        using (var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite))
        using (var reader=new StreamReader(stream,Encoding.UTF8)) return reader.ReadToEnd();
    }
    public static string ReadClosedText(string path) {
        if (!File.Exists(path)) return null;
        // The D9/D11 probes write with FileShare.None. File existence can precede
        // FlushFileBuffers/CloseHandle, so retry until a read open succeeds.
        try {
            using (var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read))
            using (var reader=new StreamReader(stream,Encoding.UTF8)) return reader.ReadToEnd();
        } catch (IOException) { return null; }
    }
}
