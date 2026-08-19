# Translations

The launcher resolves strings at runtime through `QCoreApplication::translate`
with a non-literal source, so `lupdate` cannot maintain `translations/*.ts`.
Whatever populates them sweeps up registry fragments, environment variable
names, command-line flags and paths alongside the real UI text.

`filter-translatable.py` separates the two.

    tools/i18n/filter-translatable.py --check translations/*.ts   # CI gate
    tools/i18n/filter-translatable.py --missing translations/*.ts # missing UI literals
    tools/i18n/filter-translatable.py --list  translations/soa_launcher_en.ts
    tools/i18n/filter-translatable.py --prune translations/*.ts

`--prune` edits every file it is given the same way, and refuses to run unless
they already contain identical source strings, so the catalogues cannot drift
apart. `--check` runs in CI on Linux.

`--missing` scans literal `util::i18n::translate("...")` calls under `src/` and
fails when a source is absent from the `Launcher` context used by that helper.
It also checks that the `Launcher` context agrees across every catalogue. This
catches strings stored under the wrong Qt context, which otherwise exist in the
`.ts` files but still render in English. Dynamic sources still need manual
review.

## Markup

Inline styling never belongs in a translatable string. As an example if this was in the
catalogue:

    <h2 style='color:#4F1717; margin-top:0;'>Before entering the playtest</h2>

A translator cannot be expected to preserve that, and one mangled tag breaks
the layout. Build the markup in the source and pass the prose through a
placeholder:

    label->setText(QStringLiteral("<h2 style='...'>%1</h2>")
        .arg(util::i18n::translate("Before entering the playtest")));

Light inline emphasis inside a sentence (`<b>`, `<br>`) is fine to leave in
place. Block elements and anything carrying `style=` are rejected.
