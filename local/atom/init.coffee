# Your init script
#
# Atom will evaluate this file each time a new window is opened. It is run
# after packages are loaded/activated and after the previous editor state
# has been restored.
#
# An example hack to log to the console when each text editor is saved.
#
# atom.workspace.observeTextEditors (editor) ->
#   editor.onDidSave ->
#     console.log "Saved! #{editor.getPath()}"

atom.commands.add 'atom-text-editor', 'custom:open-companion', ->
  return unless editor = atom.workspace.getActivePaneItem()
  return unless file = editor?.buffer.file
  [proj_path, relative_path] = atom.project.relativizePath(file.path)
  return unless proj_path

  editor.save()

  base_name = file.getBaseName()

  if base_name.match(/\.h$/)
    base_name = base_name.replace /\.h$/, (match) ->
      ".c"

  if base_name.match(/\.c$/)
    if m = base_name.match(/^test_(.*)/)
      companion = m[1]
      one_to_go_to = companion
      header = companion.replace /\.c+$/, (match) ->
        ".h"
    else
      header = base_name.replace /\.c+$/, (match) ->
        ".h"
      if base_name.match(/[A-Z]/)
        base_name = base_name.replace /^[A-Z]/, (match) ->
          match.toLowerCase()
        base_name = base_name.replace /[A-Z]/, (match) ->
          "_" + match.toLowerCase()
      companion = "test_" + base_name
      one_to_go_to = companion

    for f in [header, companion]
      tried_camel = false
      companion_path = null

      until(companion_path)
        pv = atom.packages.getActivePackage('fuzzy-finder').mainModule
        paths = pv.projectPaths || pv.projectView?.filePaths
        companion_path = paths.find (el) ->
          return el.match(RegExp("\/" + f + "$"))

        break if companion_path? || tried_camel

        # try CamelCase
        f = f.replace /^[a-z]/, (match) ->
          match.toUpperCase()
        f = f.replace /_([a-z])/, (match) ->
          match[1].toUpperCase()
        tried_camel = true

      atom.workspace.open(companion_path, {searchAllPanes: true}) if companion_path

  else if base_name.match(/\.rb$/)
    if m = relative_path.match(/spec\/(.*)_spec\.rb$/)
      companion_path = 'app/' + m[1] + ".rb" if m[1]
    else
      m = relative_path.match(/app\/(.*)\.rb$/)
      companion_path = 'spec/' + m[1] + "_spec.rb" if m[1]
    companion_path = proj_path + '/' + companion_path if companion_path
    atom.workspace.open(companion_path, {searchAllPanes: true}) if companion_path
