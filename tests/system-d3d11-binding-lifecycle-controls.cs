// Pure actual-production-helper controls. No native entry, registry, KMT or GPU call.
using System;
public static class SystemD11LifecycleControls01 {
 static int checks;
 static void Check(bool v){checks++;if(!v)throw new Exception("Control "+checks);}
 static void Reject(Action a){bool rejected=false;try{a();}catch(Exception){rejected=true;}Check(rejected);}
 static byte[] Raw(uint high,uint low,ulong generation,ulong caps){var b=new byte[160];U32(b,0,0x504d5644);U32(b,8,128);U32(b,128,0x44494c56);U32(b,132,1);U32(b,136,32);U32(b,140,1);U32(b,144,low);U32(b,148,high);U32(b,152,1);Array.Copy(BitConverter.GetBytes(generation),0,b,24,8);Array.Copy(BitConverter.GetBytes(caps),0,b,16,8);return b;}
 static void U32(byte[] b,int offset,uint value){Array.Copy(BitConverter.GetBytes(value),0,b,offset,4);}
 static DxvkBindingNames01 Names(string luid,string[] names){var v=new DxvkBindingNames01{Luid=luid,Names=new DxvkBindingName01[3]};for(int i=0;i<3;i++)v.Names[i]=new DxvkBindingName01{Version=(uint)i,Name=names[i]};return v;}
 public static int Main(){
  foreach(string api in new[]{"9","9ex","10","11"})foreach(string phase in new[]{"offscreen","present"})foreach(bool apply in new[]{false,true})foreach(bool wait in new[]{false,true}){
   DxvkBindingNative01.RequireLifecycleMode(false,apply,api,phase,wait);Check(true);
   if(apply && api=="11" && phase=="offscreen" && !wait){DxvkBindingNative01.RequireLifecycleMode(true,apply,api,phase,wait);Check(true);}else Reject(delegate{DxvkBindingNative01.RequireLifecycleMode(true,apply,api,phase,wait);});
  }
  foreach(uint high in new uint[]{0,0xabcdef01})foreach(uint low in new uint[]{1,0xffffffff})foreach(ulong caps in new ulong[]{0,0xffffffffffffffff}){
   string luid=high.ToString("x8")+":"+low.ToString("x8");var raw=Raw(high,low,2,caps);var v=DxvkBindingNative01.RuntimeIdentity160(raw,luid);
   Check(v.Luid==luid && v.Generation==2 && v.Capabilities==caps && v.Raw.Length==160);raw[40]=7;Check(v.Raw[40]==0);
   DxvkBindingNative01.RequireIdentityMatch(v,DxvkBindingNative01.RuntimeIdentity160(v.Raw,luid));Check(true);
  }
  string original="00000000:00006bec",forward="00000000:000084df",restored="00000000:000092ab";
  var a=DxvkBindingNative01.RuntimeIdentity160(Raw(0,0x6bec,2,0),original);var b=DxvkBindingNative01.RuntimeIdentity160(Raw(0,0x84df,2,0),forward);var c=DxvkBindingNative01.RuntimeIdentity160(Raw(0,0x92ab,2,0),restored);
  Check(a.Generation==b.Generation && b.Generation==c.Generation);Reject(delegate{DxvkBindingNative01.RequireIdentityMatch(a,b);});Reject(delegate{DxvkBindingNative01.RequireIdentityMatch(b,c);});
  foreach(int offset in new[]{0,4,8,12,128,132,136,140,144,148,152,156}){var bad=Raw(0,0x84df,2,0);bad[offset]^=1;Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(bad,forward);});}
  foreach(int offset in new[]{112,120}){var bad=Raw(0,0x84df,2,0);bad[offset]=1;Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(bad,forward);});}
  Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(Raw(0,0x84df,0,0),forward);});Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(Raw(0,0,2,0),"00000000:00000000");});
  foreach(int length in new[]{0,128,159,161})Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(new byte[length],forward);});
  Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(null,forward);});Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(b.Raw,null);});Reject(delegate{DxvkBindingNative01.RuntimeIdentity160(b.Raw,"bad");});
  foreach(int offset in new[]{16,24,40}){var changed=(byte[])b.Raw.Clone();changed[offset]^=1;var other=DxvkBindingNative01.RuntimeIdentity160(changed,forward);Reject(delegate{DxvkBindingNative01.RequireIdentityMatch(b,other);});}
  Reject(delegate{DxvkBindingNative01.RequireIdentityMatch(null,b);});Reject(delegate{DxvkBindingNative01.RequireIdentityMatch(b,null);});
  foreach(bool raw in new[]{false,true})foreach(bool worker in new[]{false,true})foreach(bool probe in new[]{false,true}){
   if(raw && worker && probe){DxvkBindingNative01.RequireLifecycleRestartPermit(raw,worker,probe);Check(true);}else Reject(delegate{DxvkBindingNative01.RequireLifecycleRestartPermit(raw,worker,probe);});
  }
  var expected=new[]{"C:\\DriverStore\\mesa.dll","C:\\DriverStore\\mesa.dll","C:\\DriverStore\\mesa.dll"};
  var good=Names(restored,expected);DxvkBindingNative01.RequireLifecycleNames(restored,expected,good);Check(true);
  Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(original,expected,good);});
  foreach(int version in new[]{0,1,2}){var bad=Names(restored,expected);bad.Names[version].Name="C:\\candidate.dll";Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,expected,bad);});bad=Names(restored,expected);bad.Names[version].Status=-1;Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,expected,bad);});bad=Names(restored,expected);bad.Names[version].Version=99;Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,expected,bad);});}
  var failed=Names(restored,expected);failed.OpenStatus=-1;Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,expected,failed);});failed=Names(restored,expected);failed.CloseStatus=-1;Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,expected,failed);});
  Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,expected,null);});Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,new string[0],good);});failed=Names(restored,expected);failed.Names[2]=null;Reject(delegate{DxvkBindingNative01.RequireLifecycleNames(restored,expected,failed);});
  DxvkBindingNative01.RequireLifecycleJobComplete(0);Check(true);foreach(uint active in new uint[]{1,2,uint.MaxValue})Reject(delegate{DxvkBindingNative01.RequireLifecycleJobComplete(active);});
  Console.WriteLine("SYSTEM_D11_LIFECYCLE_PURE_PASS checks="+checks+" native_calls=0 registry_changes=0 gpu_calls=0 hardware_admission=0");return 0;
 }
}
