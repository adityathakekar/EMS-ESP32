import { FC, useState } from 'react';
import { ValidateFieldsError } from 'async-validator';

import { Button, Checkbox, MenuItem, Grid, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { MQTT_SETTINGS_VALIDATOR, validate } from '../../validators';
import {
  BlockFormControlLabel,
  ButtonRow,
  FormLoader,
  SectionContent,
  ValidatedPasswordField,
  ValidatedTextField
} from '../../components';
import { MqttSettings } from '../../types';
import { updateValue, useRest } from '../../utils';
import * as MqttApi from '../../api/mqtt';

const MqttSettingsForm: FC = () => {
  const { loadData, saving, data, setData, saveData, errorMessage } = useRest<MqttSettings>({
    read: MqttApi.readMqttSettings,
    update: MqttApi.updateMqttSettings
  });

  const updateFormValue = updateValue(setData);

  // TODO - extend the above hook to validate the input on submit and only save to the backend if valid.
  // NB: Saving must be asserted while validation takes place
  // NB: Must also set saving to true while validating
  const [fieldErrors, setFieldErrors] = useState<ValidateFieldsError>();

  const validateAndSubmit = async () => {
    if (data) {
      try {
        setFieldErrors(undefined);
        await validate(MQTT_SETTINGS_VALIDATOR, data);
        saveData();
      } catch (errors: any) {
        setFieldErrors(errors);
      }
    }
  };

  const content = () => {
    if (!data) {
      return <FormLoader loadData={loadData} errorMessage={errorMessage} />;
    }

    return (
      <>
        <BlockFormControlLabel
          control={<Checkbox name="enabled" checked={data.enabled} onChange={updateFormValue} />}
          label="Enable MQTT?"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="host"
          label="Host"
          fullWidth
          variant="outlined"
          value={data.host}
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="port"
          label="Port"
          fullWidth
          variant="outlined"
          value={data.port}
          type="number"
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="base"
          label="Bsse"
          fullWidth
          variant="outlined"
          value={data.base}
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          name="username"
          label="Username"
          fullWidth
          variant="outlined"
          value={data.username}
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedPasswordField
          name="password"
          label="Password"
          fullWidth
          variant="outlined"
          value={data.password}
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          name="client_id"
          label="Client ID (optional)"
          fullWidth
          variant="outlined"
          value={data.client_id}
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="keep_alive"
          label="Keep Alive (seconds)"
          fullWidth
          variant="outlined"
          value={data.keep_alive}
          type="number"
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          name="mqtt_qos"
          label="QoS"
          value={data.mqtt_qos}
          fullWidth
          variant="outlined"
          onChange={updateFormValue}
          margin="normal"
          select
        >
          <MenuItem value={0}>0 (default)</MenuItem>
          <MenuItem value={1}>1</MenuItem>
          <MenuItem value={2}>2</MenuItem>
        </ValidatedTextField>
        <BlockFormControlLabel
          control={<Checkbox name="clean_session" checked={data.clean_session} onChange={updateFormValue} />}
          label="Set Clean Session"
        />
        <BlockFormControlLabel
          control={<Checkbox name="mqtt_retain" checked={data.mqtt_retain} onChange={updateFormValue} />}
          label="Always use Retain Flag"
        />

        <Typography sx={{ pt: 2 }} variant="h6" color="primary">
          Formatting
        </Typography>

        <ValidatedTextField
          name="nested_format"
          label="Topic/Payload Format"
          value={data.nested_format}
          fullWidth
          variant="outlined"
          onChange={updateFormValue}
          margin="normal"
          select
        >
          <MenuItem value={1}>Nested in a single topic</MenuItem>
          <MenuItem value={2}>As individual topics</MenuItem>
        </ValidatedTextField>
        <BlockFormControlLabel
          control={<Checkbox name="send_response" checked={data.send_response} onChange={updateFormValue} />}
          label="Publish command output to a 'response' topic"
        />
        <BlockFormControlLabel
          control={<Checkbox name="ha_enabled" checked={data.ha_enabled} onChange={updateFormValue} />}
          label="Enable Home Assistant integration"
        />
        {data.ha_enabled && (
          <ValidatedTextField
            name="ha_climate_format"
            label="Thermostat Room Temperature"
            value={data.ha_climate_format}
            fullWidth
            variant="outlined"
            onChange={updateFormValue}
            margin="normal"
            select
          >
            <MenuItem value={1}>Use Current temperature</MenuItem>
            <MenuItem value={2}>Use Setpoint temperature</MenuItem>
            <MenuItem value={3}>Always set to 0</MenuItem>
          </ValidatedTextField>
        )}

        <Typography sx={{ pt: 2 }} variant="h6" color="primary">
          Publish Intervals (in seconds, 0=automatic)
        </Typography>

        <Grid container spacing={1} direction="row" justifyContent="flex-start" alignItems="flex-start">
          <Grid item xs={4}>
            <ValidatedTextField
              fieldErrors={fieldErrors}
              name="publish_time_boiler"
              label="Boilers and Heat Pumps"
              fullWidth
              variant="outlined"
              value={data.publish_time_boiler}
              type="number"
              onChange={updateFormValue}
              margin="normal"
            />
          </Grid>

          <Grid item xs={4}>
            <ValidatedTextField
              fieldErrors={fieldErrors}
              name="publish_time_thermostat"
              label="Thermostats"
              fullWidth
              variant="outlined"
              value={data.publish_time_thermostat}
              type="number"
              onChange={updateFormValue}
              margin="normal"
            />
          </Grid>

          <Grid item xs={4}>
            <ValidatedTextField
              fieldErrors={fieldErrors}
              name="publish_time_solar"
              label="Solar Modules"
              fullWidth
              variant="outlined"
              value={data.publish_time_solar}
              type="number"
              onChange={updateFormValue}
              margin="normal"
            />
          </Grid>

          <Grid item xs={4}>
            <ValidatedTextField
              fieldErrors={fieldErrors}
              name="publish_time_mixer"
              label="Mixer Modules"
              fullWidth
              variant="outlined"
              value={data.publish_time_mixer}
              type="number"
              onChange={updateFormValue}
              margin="normal"
            />
          </Grid>

          <Grid item xs={4}>
            <ValidatedTextField
              fieldErrors={fieldErrors}
              name="publish_time_sensor"
              label="Dallas Sensors"
              fullWidth
              variant="outlined"
              value={data.publish_time_sensor}
              type="number"
              onChange={updateFormValue}
              margin="normal"
            />
          </Grid>

          <Grid item xs={4}>
            <ValidatedTextField
              fieldErrors={fieldErrors}
              name="publish_time_other"
              label="Default"
              fullWidth
              variant="outlined"
              value={data.publish_time_other}
              type="number"
              onChange={updateFormValue}
              margin="normal"
            />
          </Grid>
        </Grid>

        <ButtonRow>
          <Button
            startIcon={<SaveIcon />}
            disabled={saving}
            variant="outlined"
            color="primary"
            type="submit"
            onClick={validateAndSubmit}
          >
            Save
          </Button>
        </ButtonRow>
      </>
    );
  };

  return (
    <SectionContent title="MQTT Settings" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default MqttSettingsForm;