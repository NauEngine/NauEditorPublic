add_subdirectory(source/editor/ui)
add_subdirectory(source/editor/unit)
add_subdirectory(source/editor/usd)

set_target_properties (
  NauEditorUiTests NauEditorUnitTests NauEditorUsdTests
  PROPERTIES
    FOLDER "${NauEditorFolder}/tests"
)