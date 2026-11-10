# Local offline server. Binds only to loopback; close this window to stop.
$gameRoot = $PSScriptRoot
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback,8768)
$listener.Start()
$gamePort = $listener.LocalEndpoint.Port
Start-Process "http://127.0.0.1:$gamePort/"
Write-Host "Bot Manager: http://127.0.0.1:$gamePort/ - close this window to stop."
try {
 while ($true) {
  $client = $listener.AcceptTcpClient()
  try {
   $stream = $client.GetStream(); $stream.ReadTimeout = 5000
   $reader = [System.IO.StreamReader]::new($stream,[System.Text.Encoding]::ASCII,$false,1024,$true)
   $request = $reader.ReadLine()
   if (!$request) { continue }
   while ($reader.ReadLine()) {}
   $route = [Uri]::UnescapeDataString(($request.Split(' ')[1]).Split('?')[0])
   if ($route -eq '/') { $route = '/index.html' }
   $file = [System.IO.Path]::GetFullPath((Join-Path $gameRoot $route.TrimStart('/')))
   $status='200 OK'; $type='application/octet-stream'
   if (!$file.StartsWith($gameRoot+[System.IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or !(Test-Path -LiteralPath $file -PathType Leaf)) { $status='404 Not Found';$bytes=[Text.Encoding]::UTF8.GetBytes('Not found') }
   else {
    $bytes=[IO.File]::ReadAllBytes($file)
    switch ([IO.Path]::GetExtension($file)) { '.html' {$type='text/html; charset=utf-8'} '.js' {$type='application/javascript'} '.css' {$type='text/css'} '.json' {$type='application/json'} '.wasm' {$type='application/wasm'} '.png' {$type='image/png'} '.svg' {$type='image/svg+xml'} '.txt' {$type='text/plain'} }
   }
   $head=[Text.Encoding]::ASCII.GetBytes("HTTP/1.1 $status`r`nContent-Type: $type`r`nContent-Length: $($bytes.Length)`r`nConnection: close`r`nCache-Control: no-cache`r`n`r`n")
   $stream.Write($head,0,$head.Length);$stream.Write($bytes,0,$bytes.Length);$stream.Flush()
  } catch { Write-Host $_.Exception.Message } finally { $client.Close() }
 }
} finally { $listener.Stop() }
