/* ois.c
 *
 * Copyright (c) 2014-2015 Qualcomm Technologies, Inc. All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */

#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <media/msmb_camera.h>
#include <dlfcn.h>
#include "mct_event_stats.h"
#include "sensor_common.h"
#include "ois.h"

/** ois_power_up: Power up ois
 *  infinity position
 *
 *  @ptr: pointer to ois_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
*/
static int32_t ois_power_up(void *ptr)
{
  ois_data_t              *ois_ptr = (ois_data_t *)ptr;
  int                     rc = 0;
  struct msm_ois_cfg_data cfg;

  SDBG("enter");

  cfg.cfgtype = CFG_OIS_POWERUP;
  rc = ioctl(ois_ptr->fd, VIDIOC_MSM_OIS_CFG, &cfg);
  if (rc < 0) {
    SERR("failed: rc = %d",rc);
    return -EIO;
  }

  SDBG("exit");
  return 0;
}

/** ois_power_down: Power off ois
 *
 *  @ptr: pointer to ois_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function moves lens to infinity position **/
static int32_t ois_power_down(void *ptr)
{
  ois_data_t              *ois_ptr = (ois_data_t *)ptr;
  struct msm_ois_cfg_data cfg;
  int                     rc = 0;

  SDBG("enter");

  cfg.cfgtype = CFG_OIS_POWERDOWN;
  rc = ioctl(ois_ptr->fd, VIDIOC_MSM_OIS_CFG, &cfg);
  if (rc < 0) {
    SERR("failed: rc = %d", rc);
    return -EIO;
  }

  SDBG("exit");
  return 0;
}

/** ois_calibration: set calibration data
 *
 *  @ptr: pointer to ois_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
*/
static int32_t ois_calibration(void *ptr, void *data)
{
  int rc = 0;
  struct msm_ois_cfg_data cfg;
  ois_data_t *ois_ptr = (ois_data_t *)ptr;
  uint8_t cnt = 0;

  ois_driver_params_t *ois_driver_ptr =
    ois_ptr->ctrl->driver_ctrl;
  ois_params_t *ois_params = NULL;

  ois_params = &ois_driver_ptr->ois_params;

  cfg.cfgtype = CFG_OIS_I2C_WRITE_SEQ_TABLE;
  cfg.cfg.settings = (struct msm_camera_i2c_seq_reg_setting*)data;


  /* Invoke the IOCTL to set the ois calib data to the kernel driver */
  rc = ioctl(ois_ptr->fd, VIDIOC_MSM_OIS_CFG, &cfg);
  if (rc < 0) {
    SERR("failed rc %d", rc);
  }

  return rc;
}

/** ois_load_header: function to load the ois header
 *
 *  @ptr: pointer to ois_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function gets cam name index and initializes ois
 *  control pointer **/

static int ois_load_lib(void *ptr)
{
  uint8_t i = 0;
  int rc = 0;
  ois_driver_ctrl_t* driver_lib_data = NULL;
  ois_data_t *ois_ptr = (ois_data_t *)ptr;

  RETURN_ERR_ON_NULL(ois_ptr, -EINVAL);

  if (ois_ptr->is_ois_supported) {
    void *(*open_lib_func)(void) = NULL;
    char driver_lib_name[64];
    char driver_open_lib_func_name[64];

    RETURN_ERR_ON_NULL(ois_ptr->name, -EINVAL);

    SHIGH("name=%s", ois_ptr->name);

    if (ois_ptr->driver_lib_handle == NULL) {
      snprintf(driver_lib_name, 64, "libois_%s.so",
        ois_ptr->name);
      snprintf(driver_open_lib_func_name, 64, "ois_driver_open_lib");

      /* open ois driver library */
      ois_ptr->driver_lib_handle = dlopen(driver_lib_name, RTLD_NOW);
      RETURN_ERR_ON_NULL(ois_ptr->driver_lib_handle, -EINVAL,
        "dlopen() failed to load %s", driver_lib_name);

      *(void **)&open_lib_func = dlsym(ois_ptr->driver_lib_handle,
                                   driver_open_lib_func_name);
      RETURN_ERR_ON_NULL(open_lib_func, -EINVAL,
        "ois_driver_open_lib failed");

      driver_lib_data = (ois_driver_ctrl_t *)open_lib_func();
      RETURN_ERR_ON_NULL(driver_lib_data, -EINVAL);

      ois_ptr->ctrl->driver_ctrl =
        &(driver_lib_data->ois_driver_params);
      SLOW("library %s successfully loaded", driver_lib_name);
    }
  }
  return rc;
}

static int ois_control(void *ptr, uint16_t setting_size,
  ois_reg_settings_t *settings)
{
  int rc = 0;
  struct msm_ois_cfg_data cfg;
  ois_data_t *ois_ptr = (ois_data_t *)ptr;
  struct reg_settings_ois_t *ois_reg_settings = NULL;

  ois_driver_params_t *ois_driver_ptr =
    ois_ptr->ctrl->driver_ctrl;
  ois_params_t *ois_params = NULL;

  if (setting_size > 0) {
    ois_params = &ois_driver_ptr->ois_params;

    SLOW("configure ois settings");
    cfg.cfgtype = CFG_OIS_CONTROL;

    cfg.cfg.set_info.ois_params.i2c_addr =
      ois_params->i2c_addr;

    cfg.cfg.set_info.ois_params.i2c_freq_mode =
      sensor_sdk_util_get_i2c_freq_mode(ois_params->i2c_freq_mode);

    cfg.cfg.set_info.ois_params.setting_size = setting_size;

    ois_reg_settings = (struct reg_settings_ois_t*) malloc
        (setting_size * sizeof(struct reg_settings_ois_t));
    if(!ois_reg_settings)
    {
      SLOW("Mem allocation failed");
      return -EINVAL;
    }
    memset(ois_reg_settings, 0, sizeof(struct reg_settings_ois_t));

    translate_ois_reg_settings(ois_reg_settings, settings, setting_size);
    cfg.cfg.set_info.ois_params.settings = ois_reg_settings;

    /* Invoke the IOCTL to set the af parameters to the kernel driver */
    rc = ioctl(ois_ptr->fd, VIDIOC_MSM_OIS_CFG, &cfg);
    if (rc < 0) {
      SERR("failed rc %d", rc);
    }
  } else {
    SHIGH("failed. Ois not supported or setting size is 0.");
    /* Non-fatal */
  }

  free(ois_reg_settings);
  return rc;
}

/** ois_init: function to initialize ois
 *
 *  @ptr: pointer to ois_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function checks whether ois is supported, gets
 *  cam name index and initializes ois control pointer **/

static int ois_init(void *ptr, void* data)
{
  int rc = 0;
  ois_data_t *ois_ptr = (ois_data_t *)ptr;
  char *name = (char *)data;
  struct msm_ois_cfg_data cfg;
  ois_params_t *ois_params = NULL;

  if (ois_ptr == NULL) {
    SERR("Invalid Argument - ois_ptr");
    return -EINVAL;
  }

  SHIGH("name = %s", strlen(name) ? name : "null");

  ois_ptr->ctrl = NULL;
  ois_ptr->name = name;
  ois_ptr->is_ois_supported = (strlen(name) == 0) ? 0 : 1;

  if(!ois_ptr->is_ois_supported) {
    SHIGH("OIS not supported.");
    return 0;
  }

  ois_ptr->ctrl = malloc(sizeof(ois_ctrl_t));
  RETURN_ERR_ON_NULL(ois_ptr->ctrl, -EINVAL);

  pthread_mutex_lock(&ois_ptr->control_mutex);
  rc = ois_power_up(ois_ptr);
  if (rc < 0) {
    SERR("failed rc %d", rc);
  }

  cfg.cfgtype = CFG_OIS_INIT;

  /* Invoke the IOCTL to initialize the ois */
  rc = ioctl(ois_ptr->fd, VIDIOC_MSM_OIS_CFG, &cfg);
  if (rc < 0) {
    SERR("failed: rc = %d", rc);
    goto out;
  }

  /*load the ois library and obtain the pointer to the ois settings*/
  rc = ois_load_lib(ois_ptr);

  /*configure the ois settings*/
  ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
  rc = ois_control(ois_ptr, ois_params->init_setting_size,
    ois_params->init_settings);
  if (rc < 0) {
    SERR("failed: rc = %d", rc);
    goto out;
  }
  SLOW(" done : ois initialization return value : %d", rc);

out:
  pthread_mutex_unlock(&ois_ptr->control_mutex);

  return rc;
}

/** ois_open: function for ois open
 *
 *  @ptr: pointer to ois_data_t *
 *  @data: pointer to subdevice name
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function open subdevice and initializes ois **/

static int32_t ois_open(void **ois_ctrl, void* data)
{
  int32_t rc = 0;
  ois_data_t *ctrl = NULL;
  struct msm_ois_cfg_data cfg;
  char subdev_string[32];
  sensor_submodule_info_t *info =
      (sensor_submodule_info_t *)data;

  if (!ois_ctrl || !info) {
    SERR("failed ois_ctrl = 0x%p info = 0x%p", ois_ctrl, info);
    return -EINVAL;
  }

  if (!strlen(info->intf_info[SUBDEV_INTF_PRIMARY].sensor_sd_name)) {
    SERR("failed: sensor_sd_name is 0 length");
    return -EINVAL;
  }

  ctrl = malloc(sizeof(ois_data_t));
  RETURN_ERR_ON_NULL(ctrl, -EINVAL);

  memset(ctrl, 0, sizeof(ois_data_t));

  snprintf(subdev_string, sizeof(subdev_string), "/dev/%s",
       info->intf_info[SUBDEV_INTF_PRIMARY].sensor_sd_name);
  /* Open subdev */
  ctrl->fd = open(subdev_string, O_RDWR);
  if (ctrl->fd < 0) {
    SERR("failed to open subdev");
    rc = -EINVAL;
    goto ERROR;
  }

  *ois_ctrl = (void *)ctrl;
  return rc;

ERROR:
  free(ctrl);
  return rc;
}

/** ois_process: function to drive ois config
 *
 *  @ptr: pointer to ois_data_t
 *  @data: pointer to data sent by other modules
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function calls corresponding config function based on
 *  event type **/

static int32_t ois_process(void *ois_ctrl,
  sensor_submodule_event_type_t event, void *data)
{
  int32_t rc = 0;
  ois_data_t *ois_ptr = NULL;
  ois_params_t *ois_params = NULL;

  RETURN_ERROR_ON_NULL(ois_ctrl);

  ois_ptr = (ois_data_t *)ois_ctrl;

  switch (event) {
  /* Set params */
  case OIS_INIT:
    rc = ois_init(ois_ctrl, data);
    break;
  case OIS_ENABLE:
    if(!ois_ptr->is_ois_supported)
      break;

    RETURN_ERROR_ON_NULL(ois_ptr->ctrl);
    RETURN_ERROR_ON_NULL(ois_ptr->ctrl->driver_ctrl);
    ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
    rc = ois_control(ois_ctrl, ois_params->enable_ois_setting_size,
      ois_params->enable_ois_settings);
    break;
  case OIS_DISABLE:
    if(!ois_ptr->is_ois_supported)
      break;

    RETURN_ERROR_ON_NULL(ois_ptr->ctrl);
    RETURN_ERROR_ON_NULL(ois_ptr->ctrl->driver_ctrl);
    ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
    rc = ois_control(ois_ctrl, ois_params->disable_ois_setting_size,
      ois_params->disable_ois_settings);
    break;
  case OIS_MOVIE_MODE:
    if(!ois_ptr->is_ois_supported)
      break;

    RETURN_ERROR_ON_NULL(ois_ptr->ctrl);
    RETURN_ERROR_ON_NULL(ois_ptr->ctrl->driver_ctrl);
    ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
    rc = ois_control(ois_ctrl, ois_params->movie_mode_ois_setting_size,
      ois_params->movie_mode_ois_settings);
    break;
  case OIS_STILL_MODE:
    if(!ois_ptr->is_ois_supported)
      break;

    RETURN_ERROR_ON_NULL(ois_ptr->ctrl);
    RETURN_ERROR_ON_NULL(ois_ptr->ctrl->driver_ctrl);
    ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
    rc = ois_control(ois_ctrl, ois_params->still_mode_ois_setting_size,
      ois_params->still_mode_ois_settings);
    break;
  case OIS_CENTERING_ON:
    if(!ois_ptr->is_ois_supported)
      break;

    RETURN_ERROR_ON_NULL(ois_ptr->ctrl);
    RETURN_ERROR_ON_NULL(ois_ptr->ctrl->driver_ctrl);
    ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
    rc = ois_control(ois_ctrl, ois_params->centering_on_ois_setting_size,
      ois_params->centering_on_ois_settings);
    break;
  case OIS_CENTERING_OFF:
    if(!ois_ptr->is_ois_supported)
      break;

    RETURN_ERROR_ON_NULL(ois_ptr->ctrl);
    RETURN_ERROR_ON_NULL(ois_ptr->ctrl->driver_ctrl);
    ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
    rc = ois_control(ois_ctrl, ois_params->centering_off_ois_setting_size,
      ois_params->centering_off_ois_settings);
    break;
  case OIS_PANTILT_ON:
    if(!ois_ptr->is_ois_supported)
      break;

    RETURN_ERROR_ON_NULL(ois_ptr->ctrl);
    RETURN_ERROR_ON_NULL(ois_ptr->ctrl->driver_ctrl);
    ois_params = &ois_ptr->ctrl->driver_ctrl->ois_params;
    rc = ois_control(ois_ctrl, ois_params->pantilt_on_ois_setting_size,
      ois_params->pantilt_on_ois_settings);
    break;
  case OIS_CALIBRATION:
    if(!ois_ptr->is_ois_supported)
      break;

    rc = ois_calibration(ois_ctrl, data);
    break;
  default:
    SHIGH("invalid event %d",  event);
    rc = -EINVAL;
    break;
  }
  if (rc < 0) {
    SERR("failed: rc: %d, event: %d", rc, event);
  }
  return rc;
}

/** ois_close: function for ois close
 *
 *  @ptr: pointer to ois_data_t
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function calls close file description and frees all
 *  control data **/

static int32_t ois_close(void *ois_ctrl)
{
  int32_t rc = 0;
  ois_data_t *ctrl = (ois_data_t *)ois_ctrl;
  struct msm_ois_cfg_data cfg;

  SDBG("Enter");
  RETURN_ERROR_ON_NULL(ctrl);

  pthread_mutex_lock(&ctrl->control_mutex);

  rc = ois_power_down(ctrl);
  if (rc < 0) {
    SERR("failed rc %d", rc);
  }

  pthread_mutex_unlock(&ctrl->control_mutex);

  if (ctrl->driver_lib_handle) {
    dlclose(ctrl->driver_lib_handle);
    ctrl->driver_lib_handle = NULL;
  }

  free(ctrl->ctrl);
  /* close subdev */
  close(ctrl->fd);

  free(ctrl);

  SDBG("Exit");
  return rc;
}

/** ois_sub_module_init: function for initializing ois
 *  sub module
 *
 *  @ptr: pointer to sensor_func_tbl_t
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function initializes sub module function table with
 *  ois specific functions **/

int32_t ois_sub_module_init(sensor_func_tbl_t *func_tbl)
{
  SDBG("Enter");
  if (!func_tbl) {
    SERR("failed");
    return -EINVAL;
  }
  func_tbl->open = ois_open;
  func_tbl->process = ois_process;
  func_tbl->close = ois_close;
  return 0;
}
