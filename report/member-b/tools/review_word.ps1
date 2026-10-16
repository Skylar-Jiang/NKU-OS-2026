$ErrorActionPreference = 'Stop'
$base = 'D:\NKU-OS-2026-lab2-integration'
$docPath = Join-Path $base 'report\member-b\实验二_乙_最终版.docx'
$word = New-Object -ComObject Word.Application
$word.Visible = $false
try {
 $doc = $word.Documents.Open($docPath)
 $doc.Repaginate()
 $count = $doc.ComputeStatistics(2)
 $rows = @("PAGE_COUNT=$count")
 for ($i=1; $i -le $count; $i++) {
  $r = $doc.GoTo(1,1,$i)
  if ($i -lt $count) { $end = $doc.GoTo(1,1,($i+1)).Start } else { $end = $doc.Content.End }
  $page = $doc.Range($r.Start,$end)
  $rows += "PAGE=$i CHARS=$($page.Text.Trim().Length) INLINE_IMAGES=$($page.InlineShapes.Count)"
 }
 $doc.ExportAsFixedFormat((Join-Path $base 'report\member-b\实验二_乙_最终版.pdf'),17)
 $doc.Save()
 $rows | Set-Content -Encoding UTF8 (Join-Path $base 'report\evidence\member-b\final-review\word-pagination.txt')
 $rows
 $doc.Close(0)
} finally { $word.Quit() }
