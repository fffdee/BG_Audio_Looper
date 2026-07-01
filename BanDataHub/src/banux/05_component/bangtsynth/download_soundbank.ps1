# 音源下载工具 (Windows PowerShell版本)
# 将SF2/BGS音源文件写入soundbank.bin供主程序使用

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceFile,
    
    [Parameter(Mandatory=$false)]
    [string]$Offset = "0"
)

# 检查文件是否存在
if (-not (Test-Path $SourceFile)) {
    Write-Host "错误: 文件不存在: $SourceFile" -ForegroundColor Red
    exit 1
}

# 解析偏移地址
$offsetBytes = 0
if ($Offset.StartsWith("0x") -or $Offset.StartsWith("0X")) {
    $offsetBytes = [Convert]::ToInt64($Offset, 16)
} else {
    $offsetBytes = [Convert]::ToInt64($Offset)
}

# 获取文件大小
$fileInfo = Get-Item $SourceFile
$fileSize = $fileInfo.Length
$fileSizeMB = [math]::Round($fileSize / 1MB, 2)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "音源下载工具" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "源文件: $SourceFile"
Write-Host "文件大小: $fileSize bytes ($fileSizeMB MB)"
Write-Host "目标偏移: $Offset (0x$($offsetBytes.ToString('X8')))"
Write-Host "目标文件: soundbank.bin"
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 创建32MB的空文件(如果不存在)
$targetFile = "soundbank.bin"
if (-not (Test-Path $targetFile)) {
    Write-Host "创建 soundbank.bin (32MB)..." -ForegroundColor Yellow
    $fs = [System.IO.File]::Create($targetFile)
    $fs.SetLength(32MB)
    $fs.Close()
}

# 读取源文件数据
Write-Host "读取音源数据..." -ForegroundColor Yellow
$sourceData = [System.IO.File]::ReadAllBytes($SourceFile)

# 写入目标文件的指定偏移
Write-Host "写入 soundbank.bin (偏移: 0x$($offsetBytes.ToString('X8')))..." -ForegroundColor Yellow
$targetStream = [System.IO.File]::Open($targetFile, [System.IO.FileMode]::Open)
$targetStream.Seek($offsetBytes, [System.IO.SeekOrigin]::Begin) | Out-Null
$targetStream.Write($sourceData, 0, $sourceData.Length)
$targetStream.Close()

Write-Host ""
Write-Host "✓ 下载成功!" -ForegroundColor Green
Write-Host ""
Write-Host "后续操作:" -ForegroundColor Cyan
Write-Host "  1. 运行主程序: .\demo.exe"
Write-Host "  2. 程序会自动从 soundbank.bin 偏移 $Offset 处加载音源"
