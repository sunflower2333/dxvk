// SPDX-License-Identifier: MIT
// Metadata/file-byte checks only. This helper never loads a native module.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

public sealed class DxvkApprovedFile01 {
    public string Path;
    public long Bytes;
    public string Sha256;
}
public sealed class DxvkApprovedSelection01 {
    public string Api, Architecture, Core, CoreSha256, PrivateLoader, PrivateLoaderSha256;
    public string IcdJson, IcdJsonSha256, IcdLibrarySha256, VulkanLoaderSha256;
}
public sealed class DxvkApprovedPayload01 {
    public string SourceCommit, CiRun, Architecture, PrivateLoader, IcdLibrary;
    public DxvkApprovedFile01[] Files, ModuleFiles;
}

public static class DxvkApprovedPayloadPolicy01 {
    static readonly string[] ConfigMembers = { "vulkan_loader_config.h", "meson-build-options.json",
        "meson-build-machines.json", "native-compile-commands.json", "native-build.ninja.txt" };
    static readonly string[] ConfigSources = { ".github/workflows/build-native-umd.yml", "scripts/build-native-umd.ps1",
        "scripts/restore-native-ci-source.py", "meson.build", "meson_options.txt", "src/vulkan/meson.build",
        "src/vulkan/vulkan_loader.cpp", "src/vulkan/vulkan_loader.h", "src/umd/meson.build",
        "src/umd/umd_vulkan_loader.cpp", "src/umd/viogpudxvk.def", "scripts/owned-raw-process-f4bf37f-02.cs" };
    static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
    static Dictionary<string,object> Obj(object value) {
        var result=value as Dictionary<string,object>;
        Require(result != null,"Exact JSON object required"); return result;
    }
    static object Get(Dictionary<string,object> value, string key) {
        object result; Require(value.TryGetValue(key,out result),"Missing field: "+key); return result;
    }
    static string Str(object value) { Require(value is string,"Actual JSON string required"); return (string)value; }
    static long Int(object value) {
        Require(value is int || value is long,"Actual JSON integer required"); return Convert.ToInt64(value);
    }
    static object[] Arr(object value) {
        var result=value as object[]; Require(result != null,"Actual JSON array required"); return result;
    }
    static void Keys(Dictionary<string,object> value, params string[] keys) {
        Require(value.Count == keys.Length,"Unexpected/missing JSON fields");
        foreach (string key in keys) Require(value.ContainsKey(key),"Missing exact field: "+key);
    }
    static bool Equal(object first, object second) {
        if (first == null || second == null) return first == second;
        var left=first as Dictionary<string,object>; var right=second as Dictionary<string,object>;
        if (left != null || right != null) {
            if (left == null || right == null || left.Count != right.Count) return false;
            foreach (var row in left) if (!right.ContainsKey(row.Key) || !Equal(row.Value,right[row.Key])) return false;
            return true;
        }
        var a=first as object[]; var b=second as object[];
        if (a != null || b != null) {
            if (a == null || b == null || a.Length != b.Length) return false;
            for (int i=0;i<a.Length;++i) if (!Equal(a[i],b[i])) return false;
            return true;
        }
        return first.GetType() == second.GetType() && first.Equals(second);
    }
    public static Dictionary<string,object> Parse(string json) {
        var parser=new JavaScriptSerializer(); parser.MaxJsonLength=16*1024*1024; parser.RecursionLimit=64;
        return Obj(parser.DeserializeObject(json));
    }
    static string Sha(object value) {
        string result=Str(value); Require(Regex.IsMatch(result,"^[0-9a-f]{64}$"),"Exact SHA256 required"); return result;
    }
    static void Absolute(string value) {
        Require(!String.IsNullOrEmpty(value) && value.Length >= 4 && ((value[0]>='A' && value[0]<='Z') || (value[0]>='a' && value[0]<='z'))
            && value[1] == ':' && value[2] == '\\' && value.IndexOfAny(new char[] {'\0','"','/'}) < 0,"Normalized absolute drive path required");
        foreach (string part in value.Substring(3).Split('\\')) Require(part.Length > 0 && part != "." && part != ".."
            && part.IndexOfAny(new char[] { ':','*','?','<','>','|' }) < 0,"Ambiguous Windows path component");
    }
    static DxvkApprovedFile01 Pin(object value) {
        var row=Obj(value); Keys(row,"path","bytes","sha256");
        string path=Str(Get(row,"path")); Absolute(path);
        long bytes=Int(Get(row,"bytes")); Require(bytes > 0,"Nonempty pinned file required");
        return new DxvkApprovedFile01 { Path=path,Bytes=bytes,Sha256=Sha(Get(row,"sha256")) };
    }
    static void SamePin(DxvkApprovedFile01 file, object original) {
        var row=Obj(original);
        Require(file.Bytes == Int(Get(row,"bytes")) && file.Sha256 == Sha(Get(row,"sha256")),"Original file pin differs");
    }
    static string Parent(string path) { return path.Substring(0,path.LastIndexOf('\\')); }
    static string Leaf(string path) { return path.Substring(path.LastIndexOf('\\')+1); }
    static bool SamePath(string first,string second) { return String.Equals(first,second,StringComparison.OrdinalIgnoreCase); }
    static void Selected(DxvkApprovedFile01 file,string path,string sha) {
        Require(SamePath(file.Path,path) && file.Sha256 == sha,"Selected payload differs from approved tuple");
    }
    static Dictionary<string,object> Indexed(object rows,string field,string expected) {
        Dictionary<string,object> found=null;
        foreach (object item in Arr(rows)) {
            var row=Obj(item);
            if (Str(Get(row,field)) == expected) { Require(found == null,"Duplicate indexed row"); found=row; }
        }
        Require(found != null,"Missing indexed row: "+expected); return found;
    }
    static void True(Dictionary<string,object> value,string key) { Require(Get(value,key) is bool && (bool)Get(value,key),"Required true field: "+key); }
    static void ExactSource(Dictionary<string,object> config) {
        object[] before=Arr(Get(config,"configuration_source_before")), after=Arr(Get(config,"configuration_source_after"));
        Require(before.Length == ConfigSources.Length && after.Length == ConfigSources.Length,"Exact build configuration source closure required");
        foreach (string path in ConfigSources) {
            var a=Indexed(before,"path",path); var b=Indexed(after,"path",path);
            True(a,"matches_git"); True(b,"matches_before"); True(b,"matches_git");
            Require(Regex.IsMatch(Str(Get(a,"git_blob")),"^[0-9a-f]{40}$") && Equal(Get(a,"git_blob"),Get(a,"actual_blob")),"Source Git blob differs");
            Require(Int(Get(a,"bytes")) > 0 && Equal(Get(a,"bytes"),Get(b,"bytes")) && Sha(Get(a,"sha256")) == Sha(Get(b,"sha256")),"Source changed during build");
        }
    }
    static void Option(object[] options,string name,object expected) {
        Require(Equal(Get(Indexed(options,"name",name),"value"),expected),"Generated Meson option differs: "+name);
    }
    public static DxvkApprovedPayload01 ValidateDocuments(string tupleJson,string resultJson,string configJson,
            string header,string optionsJson,string machinesJson,string icdJson,DxvkApprovedSelection01 selection) {
        Require(selection != null && (selection.Api == "8" || selection.Api == "9" || selection.Api == "9ex" || selection.Api == "10" || selection.Api == "11"),"Exact supported API required");
        Require((selection.Api == "8" && selection.Architecture == "x86") || (selection.Api != "8" && (selection.Architecture == "arm64" || selection.Architecture == "x64")),"Exact API/probe architecture required");
        var tuple=Parse(tupleJson);
        Keys(tuple,"schema","source_commit","ci_run","ci_run_attempt","ci_repository","arch","ci_result","core","build_configuration","canonical_source","configuration_files","private_loader","icd_json","icd_metadata","icd_library","dependencies");
        Require(Str(Get(tuple,"schema")) == "system-runtime-approved-payload-v1","Reviewed payload schema required");
        string source=Str(Get(tuple,"source_commit")), run=Str(Get(tuple,"ci_run")), attempt=Str(Get(tuple,"ci_run_attempt")), arch=Str(Get(tuple,"arch"));
        Require(Regex.IsMatch(source,"^[0-9a-f]{40}$") && Regex.IsMatch(run,"^[1-9][0-9]*$") && Regex.IsMatch(attempt,"^[1-9][0-9]*$"),"Actual published source/run/attempt required; pending tuples cannot bind");
        Require(arch == selection.Architecture,"Probe and approved core architectures differ");
        var result=Parse(resultJson); True(result,"verified");
        Require(Str(Get(result,"source_commit")) == source && Convert.ToString(Get(result,"ci_run"),System.Globalization.CultureInfo.InvariantCulture) == run,"Approved CI result source/run differs");
        var binding=Obj(Get(Obj(Get(result,"core_production_bindings")),arch));
        Require(Str(Get(binding,"source_commit")) == source && Convert.ToString(Get(binding,"ci_run"),System.Globalization.CultureInfo.InvariantCulture) == run && Str(Get(binding,"arch")) == arch
            && Str(Get(binding,"producer_job_conclusion")) == "success" && Int(Get(binding,"producer_job_id")) > 0,"Original core producer binding differs");
        var core=Pin(Get(tuple,"core")); var build=Pin(Get(tuple,"build_configuration")); var canonical=Pin(Get(tuple,"canonical_source"));
        SamePin(core,Get(binding,"core")); SamePin(build,Get(binding,"build_configuration")); SamePin(canonical,Get(binding,"canonical_source"));
        Selected(core,selection.Core,selection.CoreSha256);
        var config=Parse(configJson);
        Require(Str(Get(config,"schema")) == "native-umd-build-configuration-v1" && Str(Get(config,"source_commit")) == source && Str(Get(config,"arch")) == arch
            && Str(Get(config,"library_name")) == "viogpudxvk.dll" && Str(Get(config,"loader_policy")) == "module-local-private-no-fallback","Exact current private core build configuration required");
        True(config,"all_configuration_sources_match_git"); True(config,"private_name_present_in_original_dll"); ExactSource(config);
        var github=Obj(Get(config,"github")); True(github,"actions");
        Require(Str(Get(github,"repository")) == Str(Get(tuple,"ci_repository")) && Str(Get(github,"sha")) == source
            && Str(Get(github,"run_id")) == run && Str(Get(github,"run_attempt")) == attempt,"Actual CI build identity differs");
        SamePin(core,Get(config,"dll")); Require(Str(Get(Obj(Get(config,"dll")),"member")) == "viogpudxvk.dll","Original linked core member differs");
        string loaderName=Str(Get(config,"vulkan_loader"));
        Require(loaderName == "viogpu_gl_loader_"+arch+".dll" && Str(Get(binding,"compiled_private_Vulkan_loader_name")) == loaderName,"Actual workflow configured private name differs");
        var loader=Pin(Get(tuple,"private_loader")); Selected(loader,selection.PrivateLoader,selection.PrivateLoaderSha256);
        Require(SamePath(loader.Path,Parent(core.Path)+"\\"+loaderName) && loader.Sha256 == selection.VulkanLoaderSha256,"Configured private loader must be exact module-local sibling");
        object[] rows=Arr(Get(tuple,"configuration_files"));
        Require(rows.Length == 5 && Arr(Get(config,"configuration_files")).Length == 5 && Arr(Get(binding,"actual_configuration_files")).Length == 5,"Five original configuration files required");
        var files=new List<DxvkApprovedFile01>(); files.Add(Pin(Get(tuple,"ci_result"))); files.Add(core); files.Add(build); files.Add(canonical); files.Add(loader);
        foreach (string member in ConfigMembers) {
            var selected=Indexed(rows,"member",member); Keys(selected,"member","file"); var file=Pin(Get(selected,"file"));
            SamePin(file,Indexed(Get(config,"configuration_files"),"member",member));
            SamePin(file,Get(Indexed(Get(binding,"actual_configuration_files"),"member",member),"original")); files.Add(file);
        }
        MatchCollection definitionsFound=Regex.Matches(header,@"(?m)^\s*#\s*define\s+DXVK_PRIVATE_VULKAN_LOADER\b[^\r\n]*");
        string definitions=definitionsFound.Count == 1 ? definitionsFound[0].Value.Trim() : "";
        Require(definitions == "#define DXVK_PRIVATE_VULKAN_LOADER \""+loaderName+"\"","Original generated private loader header differs");
        var parser=new JavaScriptSerializer();
        object[] options=Arr(parser.DeserializeObject(optionsJson)); Option(options,"umd_vulkan_loader",loaderName); Option(options,"umd_library_name","viogpudxvk"); Option(options,"enable_umd",true);
        var machines=Parse(machinesJson); Require(Str(Get(Obj(Get(machines,"host")),"cpu_family")) == (arch == "arm64" ? "aarch64" : arch == "x86" ? "x86" : "x86_64"),"Original Meson host architecture differs");
        var icdFile=Pin(Get(tuple,"icd_json")); Selected(icdFile,selection.IcdJson,selection.IcdJsonSha256);
        var icd=Parse(icdJson); Require(Equal(icd,Get(tuple,"icd_metadata")),"Exact approved ICD metadata differs");
        Keys(icd,"file_format_version","ICD"); string format=Str(Get(icd,"file_format_version"));
        Require(format == "1.0.0" || format == "1.0.1","Supported approved ICD format required");
        var metadata=Obj(Get(icd,"ICD"));
        if (arch == "x86") Keys(metadata,"api_version","library_path");
        else { Keys(metadata,"api_version","library_arch","library_path"); Require(Str(Get(metadata,"library_arch")) == "64","Exact approved 64-bit ICD metadata required"); }
        Require(Regex.IsMatch(Str(Get(metadata,"api_version")),"^[0-9]+\\.[0-9]+\\.[0-9]+$"),"Exact approved ICD API metadata required");
        string libraryPath=Str(Get(metadata,"library_path"));
        if (libraryPath.StartsWith(".\\",StringComparison.Ordinal)) {
            Require(Regex.IsMatch(libraryPath,@"^\.\\[A-Za-z0-9_.-]+\.dll$"),"Only one exact local ICD DLL basename is supported");
            libraryPath=Parent(icdFile.Path)+"\\"+libraryPath.Substring(2);
        }
        Absolute(libraryPath);
        var library=Pin(Get(tuple,"icd_library")); Require(SamePath(library.Path,libraryPath) && library.Sha256 == selection.IcdLibrarySha256,"Exact approved ICD library path/hash differs");
        files.Add(icdFile); files.Add(library);
        var modules=new List<DxvkApprovedFile01>(); modules.Add(core); modules.Add(loader); modules.Add(library);
        object[] dependencies=Arr(Get(tuple,"dependencies")); Require(dependencies.Length <= 32,"Bounded explicit dependency closure required");
        foreach (object item in dependencies) { var file=Pin(item); files.Add(file); modules.Add(file); }
        var paths=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var file in files) Require(paths.Add(file.Path),"Duplicate approved payload path");
        var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var file in modules) {
            string name=Leaf(file.Path).ToLowerInvariant();
            Require(name != "winevulkan.dll" && name != "vulkan-1.dll" && name != "d3d10warp.dll" && name != "d3d11warp.dll" && name != "warp.dll","Current private CI payload cannot approve public fallback/software dependencies");
            Require(names.Add(name),"Ambiguous approved module basename");
        }
        return new DxvkApprovedPayload01 { SourceCommit=source,CiRun=run,Architecture=arch,PrivateLoader=loader.Path,IcdLibrary=library.Path,Files=files.ToArray(),ModuleFiles=modules.ToArray() };
    }
    public static string FileSha(string path) {
        using (var hash=SHA256.Create()) using (var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read))
            return BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
    }
    public static void RequireFile(DxvkApprovedFile01 file) {
        Require(new FileInfo(file.Path).Length == file.Bytes && FileSha(file.Path) == file.Sha256,"Actual approved file bytes differ: "+file.Path);
    }
    static string Text(DxvkApprovedFile01 file) { RequireFile(file); return File.ReadAllText(file.Path,new UTF8Encoding(false,true)); }
    public static void RequireI386Module(byte[] bytes) {
        Require(bytes != null && bytes.Length >= 64 && bytes[0] == 0x4d && bytes[1] == 0x5a,"Actual I386 module DOS header required");
        uint offset=(uint)(bytes[60] | bytes[61]<<8 | bytes[62]<<16 | bytes[63]<<24);
        Require(offset >= 64 && offset <= bytes.Length-6 && bytes[offset] == 0x50 && bytes[offset+1] == 0x45 && bytes[offset+2] == 0 && bytes[offset+3] == 0
            && bytes[offset+4] == 0x4c && bytes[offset+5] == 1,"Actual I386 module PE machine required");
    }
    public static DxvkApprovedPayload01 ValidateFiles(string tuplePath,string tupleSha,DxvkApprovedSelection01 selection) {
        Absolute(tuplePath); Require(FileSha(tuplePath) == tupleSha && Regex.IsMatch(tupleSha ?? "","^[0-9a-f]{64}$"),"ROOT-approved tuple file hash required");
        string tupleJson=File.ReadAllText(tuplePath,new UTF8Encoding(false,true)); var tuple=Parse(tupleJson);
        string result=Text(Pin(Get(tuple,"ci_result"))), config=Text(Pin(Get(tuple,"build_configuration"))), icd=Text(Pin(Get(tuple,"icd_json")));
        string header=Text(Pin(Get(Indexed(Get(tuple,"configuration_files"),"member",ConfigMembers[0]),"file")));
        string options=Text(Pin(Get(Indexed(Get(tuple,"configuration_files"),"member",ConfigMembers[1]),"file")));
        string machines=Text(Pin(Get(Indexed(Get(tuple,"configuration_files"),"member",ConfigMembers[2]),"file")));
        var approved=ValidateDocuments(tupleJson,result,config,header,options,machines,icd,selection);
        foreach (var file in approved.Files) RequireFile(file);
        // The retained original I386 ICD JSON has no library_arch member.
        // Prove bitness from pinned module bytes without editing its metadata.
        if (approved.Architecture == "x86") foreach (var module in approved.ModuleFiles) { RequireI386Module(File.ReadAllBytes(module.Path)); RequireFile(module); }
        Require(FileSha(tuplePath) == tupleSha,"Approved tuple changed during validation"); return approved;
    }
    public static void RequireModules(DxvkApprovedFile01[] required,DxvkApprovedFile01[] observed) { RequireModuleRows(required,observed,4); }
    public static void RequireModulesForApi(string api,DxvkApprovedFile01[] required,DxvkApprovedFile01[] observed) {
        Require(api == "8" || api == "9" || api == "9ex" || api == "10" || api == "11","Reviewed module API required");
        // API8 binds the original I386 core directly, so it has no separately
        // loaded validation frontend. Core/loader/ICD remain mandatory.
        RequireModuleRows(required,observed,api == "8" ? 3 : 4);
    }
    static void RequireModuleRows(DxvkApprovedFile01[] required,DxvkApprovedFile01[] observed,int minimum) {
        Require(required != null && required.Length >= minimum && observed != null,"Exact held module census required");
        var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var wanted in required) {
            Absolute(wanted.Path); Require(names.Add(Leaf(wanted.Path)),"Duplicate required module name");
            int matches=0;
            foreach (var actual in observed) {
                if (!String.Equals(Leaf(actual.Path),Leaf(wanted.Path),StringComparison.OrdinalIgnoreCase)) continue;
                Require(SamePath(actual.Path,wanted.Path) && actual.Bytes == wanted.Bytes && actual.Sha256 == wanted.Sha256,"Held module path/bytes/hash differs"); ++matches;
            }
            Require(matches == 1,"One actual loaded approved module required: "+wanted.Path);
        }
        foreach (var actual in observed) {
            string name=Leaf(actual.Path).ToLowerInvariant();
            Require(name != "d3d10warp.dll" && name != "d3d11warp.dll" && name != "warp.dll","WARP module rejected");
            Require(name != "winevulkan.dll" && name != "vulkan-1.dll","Current private CI payload cannot load public Vulkan fallback modules");
        }
    }
    public static void RequireProcessJoin(int heldPid,string heldStart,long heldHandle,int originalPid,string originalStart,long originalHandle) {
        Require(heldPid > 0 && heldPid == originalPid && heldHandle > 0 && originalHandle > 0
            && !String.IsNullOrEmpty(heldStart) && heldStart == originalStart,"Held census PID/start must join the actual original retained runner");
        DateTime parsed;
        Require(DateTime.TryParseExact(heldStart,"o",System.Globalization.CultureInfo.InvariantCulture,System.Globalization.DateTimeStyles.RoundtripKind,out parsed)
            && parsed.Kind == DateTimeKind.Utc,"Exact original UTC start timestamp required");
        // Handles belong to separate retained Process objects; numeric equality
        // is neither required nor evidence of an original process identity.
    }
}
