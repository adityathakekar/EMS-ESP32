import { FC, useState } from 'react';
import { ValidateFieldsError } from 'async-validator';

import { Button, Checkbox, MenuItem } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { validate } from '../../validators';
import { BlockFormControlLabel, ButtonRow, FormLoader, SectionContent, ValidatedTextField } from '../../components';
import { NTPSettings } from '../../types';
import { updateValue, useRest } from '../../utils';
import * as NTPApi from '../../api/ntp';
import { selectedTimeZone, timeZoneSelectItems, TIME_ZONES } from './TZ';
import { NTP_SETTINGS_VALIDATOR } from '../../validators/ntp';

const NTPSettingsForm: FC = () => {
  const { loadData, saving, data, setData, saveData, errorMessage } = useRest<NTPSettings>({
    read: NTPApi.readNTPSettings,
    update: NTPApi.updateNTPSettings
  });

  const updateFormValue = updateValue(setData);

  const [fieldErrors, setFieldErrors] = useState<ValidateFieldsError>();

  const content = () => {
    if (!data) {
      return <FormLoader onRetry={loadData} errorMessage={errorMessage} />;
    }

    const validateAndSubmit = async () => {
      try {
        setFieldErrors(undefined);
        await validate(NTP_SETTINGS_VALIDATOR, data);
        saveData();
      } catch (errors: any) {
        setFieldErrors(errors);
      }
    };

    const changeTimeZone = (event: React.ChangeEvent<HTMLInputElement>) => {
      setData({
        ...data,
        tz_label: event.target.value,
        tz_format: TIME_ZONES[event.target.value]
      });
    };

    return (
      <>
        <BlockFormControlLabel
          control={<Checkbox name="enabled" checked={data.enabled} onChange={updateFormValue} />}
          label="Enable NTP"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="server"
          label="Server"
          fullWidth
          variant="outlined"
          value={data.server}
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="tz_label"
          label="Time zone"
          fullWidth
          variant="outlined"
          value={selectedTimeZone(data.tz_label, data.tz_format)}
          onChange={changeTimeZone}
          margin="normal"
          select
        >
          <MenuItem disabled>Time zone...</MenuItem>
          {timeZoneSelectItems()}
        </ValidatedTextField>
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
    <SectionContent title="NTP Settings" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default NTPSettingsForm;
