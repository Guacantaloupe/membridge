param(
    [string]$Path = '\Device',
    [string]$Filter = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System; using System.Collections.Generic; using System.Runtime.InteropServices;
public static class MbObjEnum {
 [StructLayout(LayoutKind.Sequential)] public struct UNICODE_STRING { public ushort Length; public ushort MaximumLength; public IntPtr Buffer; }
 [StructLayout(LayoutKind.Sequential)] public struct OBJECT_ATTRIBUTES { public uint Length; public IntPtr RootDirectory; public IntPtr ObjectName; public uint Attributes; public IntPtr SecurityDescriptor; public IntPtr SecurityQualityOfService; }
 [StructLayout(LayoutKind.Sequential)] public struct OBJECT_DIRECTORY_INFORMATION { public UNICODE_STRING Name; public UNICODE_STRING TypeName; }
 [DllImport("ntdll.dll")] static extern int NtOpenDirectoryObject(out IntPtr h, uint access, ref OBJECT_ATTRIBUTES oa);
 [DllImport("ntdll.dll")] static extern int NtQueryDirectoryObject(IntPtr h, IntPtr buf, uint len, bool single, bool restart, out uint ctx, out uint retLen);
 [DllImport("ntdll.dll")] static extern int NtClose(IntPtr h);
 public static string[] List(string path) {
  var list = new List<string>(); IntPtr nameBuf = Marshal.StringToHGlobalUni(path);
  try {
   var us = new UNICODE_STRING(); us.Length=(ushort)(path.Length*2); us.MaximumLength=(ushort)(path.Length*2+2); us.Buffer=nameBuf;
   var oa = new OBJECT_ATTRIBUTES(); oa.Length=(uint)Marshal.SizeOf(typeof(OBJECT_ATTRIBUTES)); oa.ObjectName=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(UNICODE_STRING)));
   Marshal.StructureToPtr(us, oa.ObjectName, false);
   IntPtr h; int st=NtOpenDirectoryObject(out h,0x1,ref oa); Marshal.FreeHGlobal(oa.ObjectName);
   if(st!=0) return new string[]{"ERR_OPEN "+st};
   try { IntPtr buf=Marshal.AllocHGlobal(0x4000); try { uint ctx=0,ret=0; bool restart=true;
    for(int i=0;i<8192;i++){ st=NtQueryDirectoryObject(h,buf,0x4000,true,restart,out ctx,out ret); restart=false; if(st!=0) break;
     var info=(OBJECT_DIRECTORY_INFORMATION)Marshal.PtrToStructure(buf,typeof(OBJECT_DIRECTORY_INFORMATION)); if(info.Name.Buffer==IntPtr.Zero) break;
     string nm=Marshal.PtrToStringUni(info.Name.Buffer,info.Name.Length/2);
     string tn=info.TypeName.Buffer==IntPtr.Zero?"":Marshal.PtrToStringUni(info.TypeName.Buffer,info.TypeName.Length/2);
     list.Add(nm+"`t"+tn);
    } } finally { Marshal.FreeHGlobal(buf); } } finally { NtClose(h); }
  } finally { Marshal.FreeHGlobal(nameBuf); }
  return list.ToArray();
 }
}
'@
$items = [MbObjEnum]::List($Path)
if ($Filter) { $items = $items | Where-Object { $_ -match $Filter } }
$items | ForEach-Object {
    $parts = $_ -split "`t", 2
    [pscustomobject]@{ Name=$parts[0]; Type=if($parts.Count -gt 1){$parts[1]}else{''} }
} | Format-Table -AutoSize
