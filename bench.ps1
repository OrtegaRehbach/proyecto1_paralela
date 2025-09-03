
param(
  [string] $Config = "Release",
  [string] $Preset = "msvc-release",
  [string] $OutCsv = "bench_results.csv",
  [int]    $Reps = 10
)

$ErrorActionPreference = "Stop"

# 1) Configurar y compilar
Write-Host "== Configure with preset: $Preset =="
cmake --preset $Preset | Out-Host

Write-Host "== Build ($Config) =="
cmake --build --preset build-$($Config.ToLower()) | Out-Host

# 2) Ejecutable
$exe = Join-Path -Path (Resolve-Path ".\build").Path -ChildPath "$Config\saver.exe"
if (!(Test-Path $exe)) {
  Write-Error "No se encontró el ejecutable: $exe"
}

# 3) Parámetros de prueba (ajusta a gusto)
$Ns       = @(1000, 5000, 10000, 20000)
$Threads  = @(1, 2, 4, 8)
$Modes    = @("seq", "omp")
$Seconds  = 3
$Seed     = 42                 # <- seed fija para TODAS las iteraciones

# 4) CSV header
"mode,N,threads,rep,time,ups" | Out-File -Encoding utf8 $OutCsv

# 5) Ejecutar matriz
foreach ($n in $Ns) {
  foreach ($m in $Modes) {

    # Para 'seq' solo threads=1; para 'omp' usa la lista
    $threadsList = if ($m -eq "seq") { @(1) } else { $Threads }

    foreach ($t in $threadsList) {
      for ($r = 1; $r -le $Reps; $r++) {
        $args = @("--mode", $m, "-n", $n, "--benchmark", $Seconds, "--seed", $Seed)
        if ($m -eq "omp") {
          $args += @("--threads", $t)
        }

        Write-Host ">> $($exe) $($args -join ' ')"
        $output = & $exe @args
        $line = $output | Select-String -Pattern "^\[BENCH\].*"
        if ($null -eq $line) {
          Write-Warning "No se encontró línea [BENCH] en la salida."
          continue
        }

        $text = $line.ToString()
        $time = [regex]::Match($text, "time=([0-9.]+)s").Groups[1].Value
        $ups  = [regex]::Match($text, "UPS=([0-9.]+)").Groups[1].Value

        "$m,$n,$t,$r,$time,$ups" | Out-File -Append -Encoding utf8 $OutCsv
      }
    }
  }
}

Write-Host "== Listo =="
Write-Host "Resultados en: $OutCsv"
