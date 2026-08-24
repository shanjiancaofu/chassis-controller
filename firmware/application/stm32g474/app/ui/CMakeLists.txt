add_library(chassis_ui STATIC
  lcd/lcd_ui.c
  lcd/lcd_status_presenter.c)

chassis_target_setup(chassis_ui)
target_link_libraries(chassis_ui PUBLIC chassis_drivers chassis_product)
