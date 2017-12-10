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

  if base_name.match(/\.c$/)
    if m = base_name.match(/^Test(.*)/)
      companion = m[1]
    else
      companion = "Test" + base_name
    pv = atom.packages.getActivePackage('fuzzy-finder').mainModule
    paths = pv.projectPaths || pv.projectView?.filePaths
    companion_path = paths.find (el) ->
      return el.match(RegExp("\/" + companion + "$"))

  else if base_name.match(/\.rb$/)
    if m = relative_path.match(/spec\/(.*)_spec\.rb$/)
      companion_path = 'app/' + m[1] + ".rb" if m[1]
    else
      m = relative_path.match(/app\/(.*)\.rb$/)
      companion_path = 'spec/' + m[1] + "_spec.rb" if m[1]
    companion_path = proj_path + '/' + companion_path if companion_path

  atom.workspace.open(companion_path, {searchAllPanes: true}) if companion_path
