import pathlib


def add_visual_sections_to_report(report_path: pathlib.Path):
    if not report_path.exists():
        return

    html = report_path.read_text(encoding="utf-8")
    style_block = """
<style id="cpddl-report-style">
.cpddl-field-legend{display:flex;gap:10px;flex-wrap:wrap;margin:12px 0 16px}
.cpddl-chip{padding:4px 10px;border-radius:999px;font-size:12px;font-weight:600}
.cpddl-chip-meta{background:#eef2ff;color:#3730a3}
.cpddl-chip-quality{background:#ecfdf5;color:#065f46}
.cpddl-chip-time{background:#fff7ed;color:#9a3412}
.cpddl-chip-search{background:#fef2f2;color:#991b1b}
.cpddl-jumpbar{display:flex;gap:8px;flex-wrap:wrap;margin:0 0 14px}
.cpddl-jumpbar a{padding:4px 10px;border-radius:8px;text-decoration:none;font-weight:600}
.cpddl-jump-quality{background:#ecfdf5;color:#065f46}
.cpddl-jump-time{background:#fff7ed;color:#9a3412}
.cpddl-jump-search{background:#fef2f2;color:#991b1b}
table thead th{position:sticky;top:0;background:#fff;z-index:1}
section h2{padding:8px 10px;border-radius:8px;border-left:6px solid transparent}
section.cpddl-sec-quality h2{background:#ecfdf5;border-left-color:#10b981}
section.cpddl-sec-time h2{background:#fff7ed;border-left-color:#f97316}
section.cpddl-sec-search h2{background:#fef2f2;border-left-color:#ef4444}
section.cpddl-sec-meta h2{background:#eef2ff;border-left-color:#4f46e5}
</style>
"""
    script_block = """
<script id="cpddl-report-script">
(function(){
  const sectionGroups = {
    quality: ['coverage','error','trivially_unsolvable'],
    time: ['time_total','search_time','total_time','times'],
    search: ['found_plan_length','evaluations'],
    meta: ['info','summary']
  };
  const sections = Array.from(document.querySelectorAll('section[id]'));
  const normalize = (s) => (s || '').replace(/<wbr\\s*\\/?>(?!$)/gi, '').replace(/_/g,'_').trim().toLowerCase();
  const inGroup = (id, names) => names.some(name => id === name || id.startsWith(name + '-'));
  sections.forEach(sec => {
    const id = normalize(sec.id || '');
    if (inGroup(id, sectionGroups.quality)) sec.classList.add('cpddl-sec-quality');
    else if (inGroup(id, sectionGroups.time)) sec.classList.add('cpddl-sec-time');
    else if (inGroup(id, sectionGroups.search)) sec.classList.add('cpddl-sec-search');
    else if (inGroup(id, sectionGroups.meta)) sec.classList.add('cpddl-sec-meta');
  });
})();
</script>
"""
    legend_block = """
<div class="cpddl-jumpbar">
  <a class="cpddl-jump-quality" href="#coverage">Quality</a>
  <a class="cpddl-jump-time" href="#time_total">Timing</a>
  <a class="cpddl-jump-search" href="#times">Search/plan stats</a>
</div>
<div class="cpddl-field-legend">
  <span class="cpddl-chip cpddl-chip-quality">Quality/outcome</span>
  <span class="cpddl-chip cpddl-chip-time">Timing fields</span>
  <span class="cpddl-chip cpddl-chip-search">Search stats</span>
  <span class="cpddl-chip cpddl-chip-meta">Info/summary</span>
</div>
"""

    if "cpddl-report-style" not in html and "</head>" in html:
        html = html.replace("</head>", f"{style_block}\n</head>", 1)
    if "cpddl-report-script" not in html and "</body>" in html:
        html = html.replace("</body>", f"{script_block}\n</body>", 1)
    if "cpddl-field-legend" not in html and "<body>" in html:
        html = html.replace("<body>", f"<body>\n{legend_block}", 1)

    report_path.write_text(html, encoding="utf-8")


def style_all_generated_reports(base_path: pathlib.Path):
    report_files = list((base_path / "data").glob("*-eval/report*.html"))
    for report_path in report_files:
        add_visual_sections_to_report(report_path)
