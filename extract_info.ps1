$OutFile = "c:\TouchGFXProjects\MyApplication_29\project_dump.md"

"## 1. Contenu COMPLET du fichier .ioc" | Out-File -FilePath $OutFile -Encoding utf8
"```ini" | Out-File -FilePath $OutFile -Append -Encoding utf8
Get-Content -Path "c:\TouchGFXProjects\MyApplication_29\STM32H747I-DISCO.ioc" | Out-File -FilePath $OutFile -Append -Encoding utf8
"```" | Out-File -FilePath $OutFile -Append -Encoding utf8

"## 2. Liste de tous les fichiers .ld (linker scripts)" | Out-File -FilePath $OutFile -Append -Encoding utf8
$LdFiles = Get-ChildItem -Recurse -Filter *.ld
foreach ($file in $LdFiles) {
    "### $($file.FullName)" | Out-File -FilePath $OutFile -Append -Encoding utf8
    "```ld" | Out-File -FilePath $OutFile -Append -Encoding utf8
    Get-Content -Path $file.FullName | Out-File -FilePath $OutFile -Append -Encoding utf8
    "```" | Out-File -FilePath $OutFile -Append -Encoding utf8
}

"## 7. Fichiers main.c de CM7 et CM4" | Out-File -FilePath $OutFile -Append -Encoding utf8
"### CM7 main.c" | Out-File -FilePath $OutFile -Append -Encoding utf8
"```c" | Out-File -FilePath $OutFile -Append -Encoding utf8
Get-Content -Path "c:\TouchGFXProjects\MyApplication_29\CM7\Core\Src\main.c" | Out-File -FilePath $OutFile -Append -Encoding utf8
"```" | Out-File -FilePath $OutFile -Append -Encoding utf8

"### CM4 main.c" | Out-File -FilePath $OutFile -Append -Encoding utf8
"```c" | Out-File -FilePath $OutFile -Append -Encoding utf8
Get-Content -Path "c:\TouchGFXProjects\MyApplication_29\CM4\Core\Src\main.c" | Out-File -FilePath $OutFile -Append -Encoding utf8
"```" | Out-File -FilePath $OutFile -Append -Encoding utf8

Write-Output "Done"
