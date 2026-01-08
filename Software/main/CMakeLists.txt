idf_component_register(
    SRCS
    "main.c"   
    INCLUDE_DIRS "."
    REQUIRES 
        esp_adc
        nvs_flash
        esp_driver_tsens        
        dlms
        esp_timer
        esp_app_format
    

    
)
