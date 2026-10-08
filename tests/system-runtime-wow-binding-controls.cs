// SPDX-License-Identifier: MIT
// New API8/WoW memory controls. No registry, Windows process, KMT or GPU calls.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

public static class SystemRuntimeWowBindingControls01 {
    static int checks;
    static void Check(bool pass) { ++checks; if (!pass) throw new Exception("WoW control "+checks); }
    static void Reject(Action action) { bool failed=false;try { action(); } catch(ArgumentException) {failed=true;} catch(InvalidOperationException) {failed=true;} Check(failed); }
    static byte[] Pe(uint machine) { var b=new byte[134];b[0]=0x4d;b[1]=0x5a;b[60]=128;b[128]=0x50;b[129]=0x45;b[132]=(byte)machine;b[133]=(byte)(machine>>8);return b; }
    static DxvkBindingRawValue01 Row(uint view,string name,string text) { return new DxvkBindingRawValue01 {View=view,Name=name,Exists=true,Type=7,Data=Encoding.Unicode.GetBytes(text)}; }
    static DxvkBindingRawValue01 Copy(DxvkBindingRawValue01 row) { return new DxvkBindingRawValue01 {View=row.View,Name=row.Name,Exists=row.Exists,Type=row.Type,Data=(byte[])row.Data.Clone()}; }
    static DxvkBindingRawValue01[] Copy(DxvkBindingRawValue01[] rows) {var result=new DxvkBindingRawValue01[rows.Length];for(int i=0;i<rows.Length;i++)result[i]=Copy(rows[i]);return result;}
    static void Selectors() {
        const string original=@"C:\Mesa\d9.dll",d10=@"C:\Mesa\d10.dll",d11=@"C:\Mesa\d11.dll";
        foreach(string candidate in new[]{@"C:\V\viogpudxvk.dll",@"C:\Reviewed\x86\long-core.dll",@"C:\測\試\core.dll"})
        foreach(string suffix in new[]{"\0\0","\0\0\0","\0\0\0\0\0\0"}) foreach(bool alias in new[]{false,true}) {
            var saved=new DxvkBindingRawValue01[6];int n=0;
            foreach(uint view in new uint[]{0x100,0x200}) foreach(string name in new[]{"UserModeDriverName","UserModeDriverNameWoW","InstalledDisplayDrivers"}) saved[n++]=Row(view,name,original+"\0"+d10+"\0"+d11+suffix);
            var before=Copy(saved);var candidateRow=DxvkBindingNative01.ReplaceWowLegacy(saved[1],candidate);
            Check(candidateRow.View==0x100 && candidateRow.Name=="UserModeDriverNameWoW");
            Check(DxvkBindingNative01.Same(saved[1],before[1]));
            Check(Encoding.Unicode.GetString(candidateRow.Data)==candidate+"\0"+d10+"\0"+d11+suffix);
            var names=DxvkBindingNative01.Tuple(candidateRow);Check(names[0]==candidate && names[1]==d10 && names[2]==d11);
            var changed=Copy(saved);changed[1]=candidateRow;if(alias){changed[4]=Copy(candidateRow);changed[4].View=0x200;}
            DxvkBindingNative01.RequireWowMutation(saved,changed,candidateRow);Check(true);
            foreach(int index in new[]{0,2,3,5}) Check(DxvkBindingNative01.Same(changed[index],saved[index]));
            // Saved-byte replay includes both views and original terminator tails.
            var replay=Copy(saved);DxvkBindingNative01.RequireSnapshot(before,replay);Check(true);
            Reject(delegate {DxvkBindingNative01.RequireSnapshot(before,changed);});
            foreach(int index in new[]{0,2,3,5}) {var corrupt=Copy(changed);corrupt[index].Data[6]^=1;Reject(delegate {DxvkBindingNative01.RequireWowMutation(saved,corrupt,candidateRow);});}
            var badAlias=Copy(changed);badAlias[4].Data[6]^=1;Reject(delegate {DxvkBindingNative01.RequireWowMutation(saved,badAlias,candidateRow);});
        }
        var wow=Row(0x100,"UserModeDriverNameWoW",original+"\0"+d10+"\0"+d11+"\0\0");
        Reject(delegate {DxvkBindingNative01.ReplaceNativeSlot(wow,@"C:\Candidate\core.dll",0);});
        var wrong=Copy(wow);wrong.View=0x200;Reject(delegate {DxvkBindingNative01.ReplaceWowLegacy(wrong,@"C:\Candidate\core.dll");});
        wrong=Copy(wow);wrong.Name="UserModeDriverName";Reject(delegate {DxvkBindingNative01.ReplaceWowLegacy(wrong,@"C:\Candidate\core.dll");});
        Reject(delegate {DxvkBindingNative01.ReplaceWowLegacy(wow,@"C:\Candidate\..\core.dll");});
        Reject(delegate {DxvkBindingNative01.ReplaceWowLegacy(null,@"C:\Candidate\core.dll");});
        Check(DxvkBindingNative01.ProbeArchitectureForApi("8",Pe(0x14c))=="x86");
        Check(DxvkBindingNative01.BindingValueName("8","x86")=="UserModeDriverNameWoW");
        foreach(string api in new[]{"9","9ex","10","11"}) foreach(string arch in new[]{"arm64","x64"}) {
            Check(DxvkBindingNative01.BindingValueName(api,arch)=="UserModeDriverName");
            Check(DxvkBindingNative01.ProbeArchitectureForApi(api,Pe(arch=="arm64"?0xaa64u:0x8664u))==arch);
            Reject(delegate {DxvkBindingNative01.BindingValueName(api,"x86");});
            Reject(delegate {DxvkBindingNative01.ProbeArchitectureForApi(api,Pe(0x14c));});
        }
        foreach(uint machine in new uint[]{0,0xaa64,0x8664,0xa641,0xffff}) Reject(delegate {DxvkBindingNative01.ProbeArchitectureForApi("8",Pe(machine));});
        foreach(string arch in new string[]{null,"","arm64","x64","x86_64"}) Reject(delegate {DxvkBindingNative01.BindingValueName("8",arch);});
        foreach(string api in new string[]{null,"","7","12"}) Reject(delegate {DxvkBindingNative01.BindingValueName(api,"x86");});
        foreach(string phase in new[]{"offscreen","present"}) foreach(bool apply in new[]{false,true}) foreach(bool wait in new[]{false,true}) {
            if(apply && !wait) {DxvkBindingNative01.RequireLifecycleMode(true,apply,"8",phase,wait);Check(true);}else Reject(delegate {DxvkBindingNative01.RequireLifecycleMode(true,apply,"8",phase,wait);});
        }
        foreach(string phase in new string[]{null,"","invalid"}) Reject(delegate {DxvkBindingNative01.RequireLifecycleMode(true,true,"8",phase,false);});
        Check(DxvkBindingNative01.LuidBytesForProbe("00000000:0345c596")=="96c5450300000000");
        Check(DxvkBindingNative01.LuidBytesForProbe("abcdef01:12345678")=="7856341201efcdab");
        Reject(delegate {DxvkBindingNative01.LuidBytesForProbe("00000000:00000000");});
    }
    static readonly JavaScriptSerializer Json=new JavaScriptSerializer {MaxJsonLength=16*1024*1024};
    static Dictionary<string,object> D(object value){return (Dictionary<string,object>)value;}
    static Dictionary<string,object> Parse(string value){return DxvkApprovedPayloadPolicy01.Parse(value);}
    static string J(object value){return Json.Serialize(value);}
    static object Pin(string path,long bytes,string sha){return new Dictionary<string,object>{{"path",path},{"bytes",bytes},{"sha256",sha}};}
    static void Payload32(string[] args) {
        if(args.Length!=5)throw new ArgumentException("Retained ARM64 configuration/header/options/machines/ICD metadata paths required for memory-only32 branch");
        string config=File.ReadAllText(args[0]).Replace("viogpu_gl_loader_arm64.dll","viogpu_gl_loader_x86.dll");
        var c=Parse(config);c["arch"]="x86";config=J(c);
        string header=File.ReadAllText(args[1]).Replace("viogpu_gl_loader_arm64.dll","viogpu_gl_loader_x86.dll");
        string options=File.ReadAllText(args[2]).Replace("viogpu_gl_loader_arm64.dll","viogpu_gl_loader_x86.dll");
        string machines=File.ReadAllText(args[3]).Replace("aarch64","x86");
        var metadata=Parse(File.ReadAllText(args[4]));D(metadata["ICD"]).Remove("library_arch");D(metadata["ICD"])["library_path"]=".\\viogpu_gl_vk_x86.dll";string icd=J(metadata);
        var github=D(c["github"]);var dll=D(c["dll"]);const string root="C:\\MemoryOnly\\";string fakeSha=new string('1',64);
        object core=Pin(root+"viogpudxvk.dll",Convert.ToInt64(dll["bytes"]),(string)dll["sha256"]),build=Pin(root+"build.json",1,fakeSha),canonical=Pin(root+"source.json",1,fakeSha);
        var rows=new List<object>();var actual=new List<object>();
        foreach(object item in (object[])c["configuration_files"]) {var row=D(item);string member=(string)row["member"];object file=Pin(root+member,Convert.ToInt64(row["bytes"]),(string)row["sha256"]);rows.Add(new Dictionary<string,object>{{"member",member},{"file",file}});actual.Add(new Dictionary<string,object>{{"member",member},{"original",file}});}
        var binding=new Dictionary<string,object>{{"source_commit",c["source_commit"]},{"ci_run",github["run_id"]},{"arch","x86"},{"producer_job_id",1},{"producer_job_conclusion","success"},{"core",core},{"build_configuration",build},{"canonical_source",canonical},{"actual_configuration_files",actual.ToArray()},{"compiled_private_Vulkan_loader_name","viogpu_gl_loader_x86.dll"}};
        string result=J(new Dictionary<string,object>{{"verified",true},{"source_commit",c["source_commit"]},{"ci_run",github["run_id"]},{"core_production_bindings",new Dictionary<string,object>{{"x86",binding}}}});
        string tuple=J(new Dictionary<string,object>{{"schema","system-runtime-approved-payload-v1"},{"source_commit",c["source_commit"]},{"ci_run",github["run_id"]},{"ci_run_attempt",github["run_attempt"]},{"ci_repository",github["repository"]},{"arch","x86"},{"ci_result",Pin(root+"result.json",1,fakeSha)},{"core",core},{"build_configuration",build},{"canonical_source",canonical},{"configuration_files",rows.ToArray()},{"private_loader",Pin(root+"viogpu_gl_loader_x86.dll",1,fakeSha)},{"icd_json",Pin(root+"icd.json",1,fakeSha)},{"icd_metadata",metadata},{"icd_library",Pin(root+"viogpu_gl_vk_x86.dll",1,fakeSha)},{"dependencies",new object[0]}});
        var selection=new DxvkApprovedSelection01 {Api="8",Architecture="x86",Core=root+"viogpudxvk.dll",CoreSha256=(string)dll["sha256"],PrivateLoader=root+"viogpu_gl_loader_x86.dll",PrivateLoaderSha256=fakeSha,VulkanLoaderSha256=fakeSha,IcdJson=root+"icd.json",IcdJsonSha256=fakeSha,IcdLibrarySha256=fakeSha};
        var approved=DxvkApprovedPayloadPolicy01.ValidateDocuments(tuple,result,config,header,options,machines,icd,selection);
        Check(approved.Architecture=="x86" && approved.ModuleFiles.Length==3 && approved.PrivateLoader==selection.PrivateLoader);
        DxvkApprovedPayloadPolicy01.RequireModulesForApi("8",approved.ModuleFiles,approved.ModuleFiles);Check(true);
        Reject(delegate {DxvkApprovedPayloadPolicy01.RequireModules(approved.ModuleFiles,approved.ModuleFiles);});
        foreach(string api in new[]{"9","9ex","10","11"}) {selection.Api=api;Reject(delegate {DxvkApprovedPayloadPolicy01.ValidateDocuments(tuple,result,config,header,options,machines,icd,selection);});}selection.Api="8";
        foreach(string arch in new[]{"arm64","x64","x86_64"}) {selection.Architecture=arch;Reject(delegate {DxvkApprovedPayloadPolicy01.ValidateDocuments(tuple,result,config,header,options,machines,icd,selection);});}selection.Architecture="x86";
        foreach(string bitness in new[]{"32","64","0","","x86"}) {var wrong=Parse(icd);D(wrong["ICD"])["library_arch"]=bitness;var t=Parse(tuple);t["icd_metadata"]=wrong;Reject(delegate {DxvkApprovedPayloadPolicy01.ValidateDocuments(J(t),result,config,header,options,machines,J(wrong),selection);});}
        Reject(delegate {DxvkApprovedPayloadPolicy01.ValidateDocuments(tuple,result,config,header,options,machines.Replace("\"x86\"","\"x86_64\""),icd,selection);});
        var pending=Parse(tuple);pending["ci_run"]=null;Reject(delegate {DxvkApprovedPayloadPolicy01.ValidateDocuments(J(pending),result,config,header,options,machines,icd,selection);});
        var failed=Parse(result);D(D(failed["core_production_bindings"])["x86"])["producer_job_conclusion"]="failure";Reject(delegate {DxvkApprovedPayloadPolicy01.ValidateDocuments(tuple,J(failed),config,header,options,machines,icd,selection);});
        foreach(string leaf in new[]{"winevulkan.dll","vulkan-1.dll","d3d10warp.dll"}) {var t=Parse(tuple);t["dependencies"]=new object[]{Pin(root+leaf,1,fakeSha)};Reject(delegate {DxvkApprovedPayloadPolicy01.ValidateDocuments(J(t),result,config,header,options,machines,icd,selection);});}
        DxvkApprovedPayloadPolicy01.RequireI386Module(Pe(0x14c));Check(true);
        foreach(uint machine in new uint[]{0,0xaa64,0x8664,0xa641,0xffff}) Reject(delegate {DxvkApprovedPayloadPolicy01.RequireI386Module(Pe(machine));});
        foreach(int length in new[]{0,63,64,128,133}) {var bad=Pe(0x14c);Array.Resize(ref bad,length);Reject(delegate {DxvkApprovedPayloadPolicy01.RequireI386Module(bad);});}
        var broken=Pe(0x14c);broken[130]=1;Reject(delegate {DxvkApprovedPayloadPolicy01.RequireI386Module(broken);});
        var shortList=new[]{approved.ModuleFiles[0],approved.ModuleFiles[1]};Reject(delegate {DxvkApprovedPayloadPolicy01.RequireModulesForApi("8",shortList,shortList);});
        var wrongModule=new DxvkApprovedFile01 {Path=root+"foreign\\viogpudxvk.dll",Bytes=approved.ModuleFiles[0].Bytes,Sha256=approved.ModuleFiles[0].Sha256};
        Reject(delegate {DxvkApprovedPayloadPolicy01.RequireModulesForApi("8",approved.ModuleFiles,new[]{wrongModule,approved.ModuleFiles[1],approved.ModuleFiles[2]});});
    }
    public static int Main(string[] args) {Selectors();Payload32(args);Console.WriteLine("SYSTEM_WOW_BINDING_MEMORY_PASS checks="+checks+" registry_calls=0 windows_process_calls=0 kmt_calls=0 gpu_calls=0 synthetic_x86_metadata=1 produced_x86_core_claim=0 hardware_admission=0");return 0;}
}
