// SPDX-License-Identifier: MIT
// Real retained USER06 JSON inputs plus synthetic metadata/census negatives.
// No native module, registry, factory or hardware call is made here.
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

public static class SystemRuntimeApprovedPayloadControls {
    static readonly JavaScriptSerializer Json=new JavaScriptSerializer { MaxJsonLength=16*1024*1024 };
    static string tuple, result, config, header, options, machines, icd;
    static int checks;
    public static int CheckCount { get { return checks; } }
    static Dictionary<string,object> D(object value) { return (Dictionary<string,object>)value; }
    static Dictionary<string,object> Parse(string value) { return DxvkApprovedPayloadPolicy01.Parse(value); }
    static string Serialize(object value) { return Json.Serialize(value); }
    static string Hash(byte[] value) { using (var h=SHA256.Create()) return BitConverter.ToString(h.ComputeHash(value)).Replace("-","").ToLowerInvariant(); }
    static string Hash(string value) { return Hash(Encoding.UTF8.GetBytes(value)); }
    static object Pin(string path,long bytes,string sha) { return new Dictionary<string,object> { {"path",path},{"bytes",bytes},{"sha256",sha} }; }
    static object PinText(string path,string value) { return Pin(path,Encoding.UTF8.GetByteCount(value),Hash(value)); }
    static void Check(bool passed,string message) { ++checks; if (!passed) throw new Exception(message); }
    static DxvkApprovedSelection01 Selection(string api) {
        var t=Parse(tuple);
        return new DxvkApprovedSelection01 { Api=api,Architecture="arm64",Core=(string)D(t["core"])["path"],CoreSha256=(string)D(t["core"])["sha256"],
            PrivateLoader=(string)D(t["private_loader"])["path"],PrivateLoaderSha256=(string)D(t["private_loader"])["sha256"],
            VulkanLoaderSha256=(string)D(t["private_loader"])["sha256"],IcdJson=(string)D(t["icd_json"])["path"],IcdJsonSha256=(string)D(t["icd_json"])["sha256"],IcdLibrarySha256=(string)D(t["icd_library"])["sha256"] };
    }
    static DxvkApprovedPayload01 Validate(string t,string r,string c,string h,string o,string m,string i,DxvkApprovedSelection01 s) {
        return DxvkApprovedPayloadPolicy01.ValidateDocuments(t,r,c,h,o,m,i,s);
    }
    static void Reject(Action action,string name) {
        bool rejected=false; try { action(); } catch (InvalidOperationException) { rejected=true; } catch (ArgumentException) { rejected=true; }
        Check(rejected,"Failed to reject "+name);
    }
    static void TupleReject(Action<Dictionary<string,object>> edit,string name) {
        var value=Parse(tuple); edit(value); Reject(delegate { Validate(Serialize(value),result,config,header,options,machines,icd,Selection("10")); },name);
    }
    static void ConfigReject(Action<Dictionary<string,object>> edit,string name) {
        var value=Parse(config); edit(value); Reject(delegate { Validate(tuple,result,Serialize(value),header,options,machines,icd,Selection("10")); },name);
    }
    static void ResultReject(Action<Dictionary<string,object>> edit,string name) {
        var value=Parse(result); edit(value); Reject(delegate { Validate(tuple,Serialize(value),config,header,options,machines,icd,Selection("10")); },name);
    }
    static void IcdReject(Action<Dictionary<string,object>> edit,string name) {
        var value=Parse(icd); edit(value); var t=Parse(tuple); t["icd_metadata"]=value;
        Reject(delegate { Validate(Serialize(t),result,config,header,options,machines,Serialize(value),Selection("10")); },name);
    }
    static DxvkApprovedFile01[] Clone(DxvkApprovedFile01[] values) {
        var result=new DxvkApprovedFile01[values.Length];
        for (int i=0;i<values.Length;++i) result[i]=new DxvkApprovedFile01 { Path=values[i].Path,Bytes=values[i].Bytes,Sha256=values[i].Sha256 }; return result;
    }
    public static int Main(string[] args) {
        if (args.Length != 5) throw new ArgumentException("Actual USER06 configuration/header/options/machines/ICD paths required");
        config=File.ReadAllText(args[0]); header=File.ReadAllText(args[1]); options=File.ReadAllText(args[2]); machines=File.ReadAllText(args[3]); icd=File.ReadAllText(args[4]);
        Check(new FileInfo(args[4]).Length == 220 && DxvkApprovedPayloadPolicy01.FileSha(args[4]) == "bd78e20731b52e6d2ded5d1c2ff23eb44127d009a72aaa50ae46a55a57d84dce","Retained ICD original differs");
        var c=Parse(config); var github=D(c["github"]); var dll=D(c["dll"]);
        const string root="C:\\Approved\\";
        object core=Pin(root+"viogpudxvk.dll",Convert.ToInt64(dll["bytes"]),(string)dll["sha256"]);
        object build=Pin(root+"native-build-configuration.json",new FileInfo(args[0]).Length,DxvkApprovedPayloadPolicy01.FileSha(args[0]));
        object canonical=PinText(root+"native-canonical-source.json","synthetic-canonical-control-only");
        var cfgRows=(object[])c["configuration_files"]; var selected=new List<object>(); var actual=new List<object>();
        foreach (object item in cfgRows) {
            var row=D(item); string member=(string)row["member"];
            object file=Pin(root+"config\\"+member,Convert.ToInt64(row["bytes"]),(string)row["sha256"]);
            selected.Add(new Dictionary<string,object> { {"member",member},{"file",file} });
            actual.Add(new Dictionary<string,object> { {"member",member},{"original",file} });
        }
        var binding=new Dictionary<string,object> { {"source_commit",c["source_commit"]},{"ci_run",github["run_id"]},{"arch","arm64"},
            {"producer_job_id",1},{"producer_job_conclusion","success"},{"core",core},{"build_configuration",build},{"canonical_source",canonical},
            {"actual_configuration_files",actual.ToArray()},{"compiled_private_Vulkan_loader_name",c["vulkan_loader"]} };
        result=Serialize(new Dictionary<string,object> { {"verified",true},{"source_commit",c["source_commit"]},{"ci_run",github["run_id"]},
            {"core_production_bindings",new Dictionary<string,object> { {"arm64",binding} }} });
        tuple=Serialize(new Dictionary<string,object> { {"schema","system-runtime-approved-payload-v1"},{"source_commit",c["source_commit"]},
            {"ci_run",github["run_id"]},{"ci_run_attempt",github["run_attempt"]},{"ci_repository",github["repository"]},{"arch","arm64"},
            {"ci_result",PinText(root+"ci-result.json",result)},{"core",core},{"build_configuration",build},{"canonical_source",canonical},{"configuration_files",selected.ToArray()},
            {"private_loader",PinText(root+(string)c["vulkan_loader"],"synthetic-private-loader-bytes")},
            {"icd_json",Pin(root+"dx10-private-icd.json",220,DxvkApprovedPayloadPolicy01.FileSha(args[4]))},{"icd_metadata",Parse(icd)},
            {"icd_library",PinText(root+"viogpu_gl_vk_arm64.dll","synthetic-ICD-library-bytes")},
            {"dependencies",new object[] { PinText(root+"zlib1.dll","synthetic-selected-dependency-bytes") }} });
        DxvkApprovedPayload01 approved=null;
        foreach (string api in new string[] {"9","9ex","10","11"}) {
            approved=Validate(tuple,result,config,header,options,machines,icd,Selection(api));
            Check(approved.Architecture == "arm64" && approved.PrivateLoader.EndsWith("viogpu_gl_loader_arm64.dll",StringComparison.Ordinal),"Exact API metadata control");
            Check(approved.Files.Length == 13 && approved.ModuleFiles.Length == 4,"Exact selected input closure");
        }
        TupleReject(delegate(Dictionary<string,object> t) { t["source_commit"]=null; },"future source");
        TupleReject(delegate(Dictionary<string,object> t) { t["ci_run"]=null; },"future run");
        TupleReject(delegate(Dictionary<string,object> t) { t["source_commit"]=new string('a',40); },"other source");
        TupleReject(delegate(Dictionary<string,object> t) { t["ci_run"]="1"; },"other CI run");
        TupleReject(delegate(Dictionary<string,object> t) { t["ci_run_attempt"]="2"; },"other attempt");
        TupleReject(delegate(Dictionary<string,object> t) { t["ci_repository"]="foreign/repo"; },"other repository");
        TupleReject(delegate(Dictionary<string,object> t) { t["arch"]="x64"; },"other architecture");
        TupleReject(delegate(Dictionary<string,object> t) { t["unreviewed"]=true; },"unexpected tuple field");
        foreach (string key in new string[] {"core","build_configuration","canonical_source"}) {
            string field=key; TupleReject(delegate(Dictionary<string,object> t) { D(t[field])["sha256"]=new string('a',64); },"original hash "+field);
            TupleReject(delegate(Dictionary<string,object> t) { D(t[field])["bytes"]=1; },"original bytes "+field);
        }
        TupleReject(delegate(Dictionary<string,object> t) { D(t["private_loader"])["path"]=root+"winevulkan.dll"; },"hardcoded winevulkan");
        TupleReject(delegate(Dictionary<string,object> t) { D(t["private_loader"])["path"]="C:\\Foreign\\viogpu_gl_loader_arm64.dll"; },"nonlocal configured loader");
        TupleReject(delegate(Dictionary<string,object> t) { D(t["icd_library"])["path"]="C:\\Foreign\\viogpu_gl_vk_arm64.dll"; },"foreign ICD library");
        TupleReject(delegate(Dictionary<string,object> t) { D(t["icd_metadata"])["file_format_version"]="1.0.0"; },"hardcoded old ICD format");
        TupleReject(delegate(Dictionary<string,object> t) { D(D(t["icd_metadata"])["ICD"])["api_version"]="1.0.0"; },"unapproved ICD API");
        TupleReject(delegate(Dictionary<string,object> t) { D(D(t["icd_metadata"])["ICD"])["library_arch"]="32"; },"unapproved ICD architecture");
        TupleReject(delegate(Dictionary<string,object> t) { D(t["icd_metadata"])["extra"]=true; },"unapproved ICD metadata");
        TupleReject(delegate(Dictionary<string,object> t) { var value=D(t["icd_metadata"]); D(value["ICD"])["library_path"]=".\\..\\foreign.dll"; },"parent relative ICD");
        TupleReject(delegate(Dictionary<string,object> t) { t["configuration_files"]=new object[0]; },"missing five generated inputs");
        TupleReject(delegate(Dictionary<string,object> t) { D(D(((object[])t["configuration_files"])[0])["file"])["sha256"]=new string('a',64); },"generated input hash");
        TupleReject(delegate(Dictionary<string,object> t) { t["dependencies"]=new object[] {t["core"]}; },"duplicate dependency");
        foreach (string fallback in new string[] { "winevulkan.dll","vulkan-1.dll","d3d10warp.dll" }) {
            string name=fallback; TupleReject(delegate(Dictionary<string,object> t) { t["dependencies"]=new object[] { PinText(root+name,"synthetic-unapproved-dependency") }; },"fallback/software dependency "+name);
        }
        IcdReject(delegate(Dictionary<string,object> x) { x["file_format_version"]="2.0.0"; },"unsupported approved format");
        IcdReject(delegate(Dictionary<string,object> x) { D(x["ICD"])["api_version"]="invalid"; },"invalid approved API metadata");
        IcdReject(delegate(Dictionary<string,object> x) { D(x["ICD"])["library_arch"]="32"; },"approved32ICD");
        IcdReject(delegate(Dictionary<string,object> x) { D(x["ICD"])["library_path"]=".\\..\\viogpu_gl_vk_arm64.dll"; },"approved parent relative path");
        IcdReject(delegate(Dictionary<string,object> x) { D(x["ICD"])["library_path"]="viogpu_gl_vk_arm64.dll"; },"approved bare relative path");
        IcdReject(delegate(Dictionary<string,object> x) { D(x["ICD"])["library_path"]=".\\sub\\viogpu_gl_vk_arm64.dll"; },"approved nested relative path");
        ConfigReject(delegate(Dictionary<string,object> x) { x["vulkan_loader"]="winevulkan.dll"; },"public loader fallback");
        ConfigReject(delegate(Dictionary<string,object> x) { x["loader_policy"]="public-search"; },"public loader policy");
        ConfigReject(delegate(Dictionary<string,object> x) { x["private_name_present_in_original_dll"]=false; },"missing private name in produced DLL");
        ConfigReject(delegate(Dictionary<string,object> x) { x["all_configuration_sources_match_git"]=false; },"unbound source inputs");
        ConfigReject(delegate(Dictionary<string,object> x) { D(x["github"])["actions"]=false; },"non-CI producer");
        ConfigReject(delegate(Dictionary<string,object> x) { D(x["github"])["sha"]=new string('a',40); },"CI source mismatch");
        ConfigReject(delegate(Dictionary<string,object> x) { D(((object[])x["configuration_source_before"])[0])["actual_blob"]=new string('a',40); },"Git blob mismatch");
        ConfigReject(delegate(Dictionary<string,object> x) { D(((object[])x["configuration_source_after"])[0])["matches_before"]=false; },"source changed during build");
        ResultReject(delegate(Dictionary<string,object> x) { x["verified"]=false; },"unverified CI originals");
        ResultReject(delegate(Dictionary<string,object> x) { D(D(x["core_production_bindings"])["arm64"])["producer_job_conclusion"]="failure"; },"failed producer");
        Reject(delegate { Validate(tuple,result,config,header+"\n#define DXVK_PRIVATE_VULKAN_LOADER \"foreign.dll\"",options,machines,icd,Selection("10")); },"duplicate generated private define");
        Reject(delegate { Validate(tuple,result,config,header.Replace("viogpu_gl_loader_arm64.dll","winevulkan.dll"),options,machines,icd,Selection("10")); },"generated name mismatch");
        Reject(delegate { Validate(tuple,result,config,header,options.Replace("viogpu_gl_loader_arm64.dll","winevulkan.dll"),machines,icd,Selection("10")); },"Meson private name mismatch");
        Reject(delegate { Validate(tuple,result,config,header,options,machines.Replace("aarch64","x86_64"),icd,Selection("10")); },"Meson architecture mismatch");
        Reject(delegate { Validate(tuple,result,config,header,options,machines,icd.Replace("1.4.354","1.4.353"),Selection("10")); },"actual ICD metadata mismatch");
        foreach (string member in new string[] {"CoreSha256","PrivateLoaderSha256","IcdJsonSha256","IcdLibrarySha256","VulkanLoaderSha256"}) {
            var s=Selection("10"); typeof(DxvkApprovedSelection01).GetField(member).SetValue(s,new string('a',64));
            Reject(delegate { Validate(tuple,result,config,header,options,machines,icd,s); },"selected "+member);
        }
        var selection=Selection("10"); selection.Core="C:\\Foreign\\viogpudxvk.dll";
        Reject(delegate { Validate(tuple,result,config,header,options,machines,icd,selection); },"selected foreign core path");
        selection=Selection("10"); selection.Architecture="x64";
        Reject(delegate { Validate(tuple,result,config,header,options,machines,icd,selection); },"selected architecture differs");
        var required=new List<DxvkApprovedFile01>(approved.ModuleFiles);
        required.Add(new DxvkApprovedFile01 { Path=root+"viogpudxvk_validate10.dll",Bytes=128,Sha256=Hash("synthetic-front") });
        var wanted=required.ToArray(); var observed=Clone(wanted);
        DxvkApprovedPayloadPolicy01.RequireModules(wanted,observed); Check(true,"matching held census");
        for (int index=0;index<wanted.Length;++index) {
            int current=index; observed=Clone(wanted); observed[current].Path="C:\\Foreign\\"+observed[current].Path.Substring(observed[current].Path.LastIndexOf('\\')+1);
            Reject(delegate { DxvkApprovedPayloadPolicy01.RequireModules(wanted,observed); },"foreign held module "+index);
            observed=Clone(wanted); observed[current].Sha256=new string('a',64);
            Reject(delegate { DxvkApprovedPayloadPolicy01.RequireModules(wanted,observed); },"changed held module "+index);
            var absent=new List<DxvkApprovedFile01>(Clone(wanted)); absent.RemoveAt(index); observed=absent.ToArray();
            Reject(delegate { DxvkApprovedPayloadPolicy01.RequireModules(wanted,observed); },"missing held module "+index);
        }
        foreach (string name in new string[] {"winevulkan.dll","vulkan-1.dll","d3d10warp.dll","d3d11warp.dll","warp.dll"}) {
            var extra=new List<DxvkApprovedFile01>(Clone(wanted)); extra.Add(new DxvkApprovedFile01 { Path=root+name }); observed=extra.ToArray();
            Reject(delegate { DxvkApprovedPayloadPolicy01.RequireModules(wanted,observed); },"foreign fallback/software "+name);
        }
        var duplicate=new List<DxvkApprovedFile01>(Clone(wanted)); duplicate.Add(Clone(wanted)[0]);
        Reject(delegate { DxvkApprovedPayloadPolicy01.RequireModules(wanted,duplicate.ToArray()); },"duplicate held module");
        const string start="2026-10-09T01:02:03.0000000Z";
        DxvkApprovedPayloadPolicy01.RequireProcessJoin(123,start,40,123,start,44); Check(true,"separate retained handles joined by PID/start");
        Reject(delegate { DxvkApprovedPayloadPolicy01.RequireProcessJoin(123,start,40,124,start,44); },"other original PID");
        Reject(delegate { DxvkApprovedPayloadPolicy01.RequireProcessJoin(123,start,40,123,"2026-10-09T01:02:04.0000000Z",44); },"reused same PID start");
        Reject(delegate { DxvkApprovedPayloadPolicy01.RequireProcessJoin(123,start,0,123,start,44); },"missing census handle");
        Reject(delegate { DxvkApprovedPayloadPolicy01.RequireProcessJoin(123,start,40,123,start,0); },"missing original handle");
        Reject(delegate { DxvkApprovedPayloadPolicy01.RequireProcessJoin(123,"invalid",40,123,"invalid",44); },"invalid joined UTC start");
        Console.WriteLine("SYSTEM approved payload controls PASS checks="+checks+" historical_icd_bytes=220 api_profiles=4 module_calls=0 registry_calls=0 hardware_admission=0");
        return 0;
    }
}
