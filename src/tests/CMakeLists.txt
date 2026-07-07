add_executable(taiga-core-tests
	core_smoke_test.cpp
)

target_link_libraries(taiga-core-tests PRIVATE
	Qt6::Core
	taiga-core
)

add_test(NAME taiga-core-smoke COMMAND taiga-core-tests)

add_executable(taiga-settings-dialog-tests
	../media/anime_list_export.cpp
	../media/anime_list_export.hpp
	../taiga/application.cpp
	../taiga/application.hpp
	../taiga/session.cpp
	../taiga/session.hpp
	settings_dialog_smoke_test.cpp
)

target_link_libraries(taiga-settings-dialog-tests PRIVATE
	Qt6::Widgets
	taiga-core
	taiga-gui
	taiga-resources
)

add_test(NAME taiga-settings-dialog-smoke COMMAND taiga-settings-dialog-tests)

add_executable(taiga-main-ui-parity-tests
	../media/anime_list_export.cpp
	../media/anime_list_export.hpp
	../taiga/application.cpp
	../taiga/application.hpp
	../taiga/session.cpp
	../taiga/session.hpp
	main_ui_parity_test.cpp
)

target_link_libraries(taiga-main-ui-parity-tests PRIVATE
	Qt6::Widgets
	taiga-core
	taiga-gui
	taiga-resources
)

add_test(NAME taiga-main-ui-parity COMMAND taiga-main-ui-parity-tests)
