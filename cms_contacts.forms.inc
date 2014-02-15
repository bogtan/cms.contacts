<?php

/**
 * @file
 *    Used to describe and build Drupal forms to add data in LDAP database
 *
 * @author
 *    Bogdan Tanase
 */

/**
 * Implements hook_form()
 *
 * Generate the Add/Edit form for LDAP contact
 *
 * @param   array   $form
 * @param   array   $form_state
 * @param   array   $contact
 *
 * @return  array   $form
 */
function cms_contacts_form($form, &$form_state, $contact) {
    if (!empty($contact)) {
        if (array_key_exists('dn', $contact) && (!empty($contact['dn']))) {
            $contact_dn = $contact['dn'];
        }
        $form_state['#contacts_edit_form'] = true;
    }else {
        $form['#contacts_edit_form'] = false;
        $contact_dn = '';
    }

    $site_uuid = variable_get('site_uuid', '');
    if (!$site_uuid) {
        drupal_set_message(t('Unable to determine site UUID! Please contact the site administrator.', 'error'));
    }

    $countries = countries_get_countries();
    $country_options[''] = t('Select country');
    foreach ($countries as $country) {
        $country_options[$country->iso2] = $country->name;
    }

    $instruments_options = CMSContacts::create_instruments_form_options();
    $contact_instruments = array();
    if ($contact_dn) {
        $contact_instruments = CMSContacts::get_groups_for_contact(
            CMSContacts::$instruments_cache, 'update_ldap_instruments_cache', $contact_dn
        );
    }

    $form['#attributes']['class'][] = 'contacts-form';
    $form['#attributes']['class'][] = 'form-horizontal';
    $form['#validate'][] = 'cms_contacts_form_validate';
    $form['#contact'] = $contact;
    $form['#tree'] = TRUE;

    // Add the buttons.
    $form['actions'] = array(
        '#type' => 'actions',
        '#prefix' => '<div class="form-actions">',
        '#suffix' => '</div>',
    );

    $form['actions']['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Save'),
        '#weight' => 5,
        '#submit' => array('cms_contacts_form_submit'),
    );

    if (!empty($contact['uid'])) {
        $form['actions']['delete'] = array(
            '#type' => 'submit',
            '#value' => t('Delete'),
            '#weight' => 15,
            '#submit' => array('cms_contacts_form_delete_submit'),
        );
    }

    $form['availability'] = array(
        '#type' => 'radios',
        '#required' => TRUE,
        '#title' => t('Availability'),
        '#default_value' => isset($contact['cmsavailability'][0]) ? $contact['cmsavailability'][0] : "TRUE",
        '#options' => array(
            "TRUE" => t('Available'),
            "FALSE" => t('Unavaiable'),
        ),
        '#label' => 'inline',
    );

    $form['personal_title'] = array(
        '#title' => t('Personal title'),
        '#type' => 'textfield',
        '#default_value' => isset($contact['personaltitle'][0]) ? $contact['personaltitle'][0] : '',
        '#size' =>  10,
        '#label' => 'inline',
        '#description' => t('e.g. Mr./Ms./Prof.'),
    );

    $form['first_name'] = array(
        '#type' => 'textfield',
        '#required' => TRUE,
        '#title' => t('First name'),
        '#default_value' => isset($contact['givenname'][0]) ? $contact['givenname'][0] : '',
        '#size' => 30,
        '#label' => 'inline'
    );

    $form['last_name'] = array(
        '#type' => 'textfield',
        '#required' => TRUE,
        '#title' => t('Last name'),
        '#default_value' => isset($contact['sn'][0]) ? $contact['sn'][0] : '',
        '#size' => 30,
    );

    $email_default = CMSContacts::get_multiple_attribute($contact, 'mail');
    $form['email'] = array(
        '#type' => 'textfield',
        '#title' => t('Work email'),
        '#description' => t('E-mail addresses from this field will be used for mailing purposes. Separate multiple entries with a comma.'),
        '#default_value' => isset($email_default) ? $email_default : '',
        '#size' => 30,
        '#maxlength' => 512,
    );

    $work_phone_default = CMSContacts::get_multiple_attribute($contact, 'workphonenumbers');
    $form['work_phone'] = array(
        '#type' => 'textfield',
        '#title' => t('Work phone'),
        '#default_value' => isset($work_phone_default) ? $work_phone_default : '',
        '#size' => 30,
        '#description' => t('Separate multiple entries with a comma.'),
        '#maxlength' => 512,
    );

    $fax_default = CMSContacts::get_multiple_attribute($contact, 'faxnumbers');
    $form['fax_number'] = array(
        '#type' => 'textfield',
        '#title' => t('Fax'),
        '#default_value' => isset($fax_default) ? $fax_default : '',
        '#size' => 30,
        '#description' => t('Separate multiple entries with a comma.'),
        '#maxlength' => 512,
    );

    $form['position'] = array(
        '#type' => 'textfield',
        '#title' => t('Position'),
        '#default_value' => isset($contact['title'][0]) ? $contact['title'][0] : '',
        '#size' => 30,
    );


    // Status person
    // Field with multiple values
    $form['instrument_fieldset'] = array(
        '#type' => 'fieldset',
        '#title' => t('CMS Instrument related details'),
        // Set up the wrapper so that AJAX will be able to replace the fieldset.
        '#prefix' => '<div id="instrument-fieldset-wrapper">',
        '#suffix' => '</div><div class="clear">&nbsp;</div><br />',
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
    );

    // Build the fieldset with the proper number of names. We'll use
    // $form_state['num_status_person'] to determine the number of textfields to build.
    $person_statuses = CMSContacts::create_form_options_for_field(CMSContacts::$status_person_cache, 'update_ldap_taxonomies_cache');
    $organization_statuses = CMSContacts::create_form_options_for_field(CMSContacts::$status_organization_cache, 'update_ldap_taxonomies_cache');
    $mailing_lists = CMSContacts::create_form_options_for_field(CMSContacts::$mailing_list_cache, 'update_ldap_taxonomies_cache');

    if (empty($form_state['num_instrument'])) {
        $form_state['num_instrument'] = (!empty($contact['instrument_data'])) ? count($contact['instrument_data']) : 1;
    }

    for ($i = 0; $i < $form_state['num_instrument']; $i++) {
        $instrument_uuid = array_keys($contact['instrument_data'])[$i];

        $form['instrument_fieldset'][$i]['instrument'] = array(
            '#type' => 'select',
            '#required' => FALSE,
            '#multiple' => FALSE,
            '#title' => t('CMS Instrument'),
            '#default_value' => isset($instrument_uuid) ? "uuid=" . $instrument_uuid . "," . variable_get(LDAP_BASE_DN) : '',
            '#options' => $instruments_options,
            '#size' => 5,
            '#attributes' => array(
                'class' => array('chosen-select-ctrl')
            ),
        );

        $form['instrument_fieldset'][$i]['person_status'] = array(
            '#type' => 'select',
            '#multiple' => TRUE,
            '#title' => t('Status person'),
            '#default_value' => (!empty($contact['instrument_data'][$instrument_uuid]['status_person'])) ? $contact['instrument_data'][$instrument_uuid]['status_person'] : '',
            '#options' => $person_statuses,
            '#size' => 5,
            '#attributes' => array(
                'class' => array('chosen-select-ctrl')
            ),
        );

        $form['instrument_fieldset'][$i]['organization_status'] = array(
            '#type' => 'select',
            '#multiple' => TRUE,
            '#title' => t('Status organization'),
            '#default_value' => (!empty($contact['instrument_data'][$instrument_uuid]['status_organization'])) ? $contact['instrument_data'][$instrument_uuid]['status_organization'] : '',
            '#options' => $organization_statuses,
            '#attributes' => array(
                'class' => array('chosen-select-ctrl')
            ),
        );

        $form['instrument_fieldset'][$i]['mailing_list'] = array(
            '#type' => 'select',
            '#multiple' => TRUE,
            '#title' => t('Mailing lists'),
            '#default_value' => (!empty($contact['instrument_data'][$instrument_uuid]['mailing_list'])) ? $contact['instrument_data'][$instrument_uuid]['mailing_list'] : '',
            '#options' => $mailing_lists,
            '#attributes' => array(
                'class' => array('chosen-select-ctrl')
            ),
            '#size' => 5,
            '#suffix' => '<br />',
        );
    }

    $form['instrument_fieldset']['add_instrument'] = array(
        '#type' => 'submit',
        '#value' => t('Add more'),
        '#name' => 'add_instrument_submit',
        '#submit' => array('cms_contacts_form_add_more_add_one_instrument'),
        '#ajax' => array(
            'callback' => 'cms_contacts_form_instrument_callback',
            'wrapper' => 'instrument-fieldset-wrapper',
        ),
    );
    if ($form_state['num_instrument'] > 1) {
        $form['instrument_fieldset']['remove_last_instrument'] = array(
            '#type' => 'submit',
            '#value' => t('Remove last added'),
            '#submit' => array('cms_contacts_form_remove_one_instrument'),
            '#ajax' => array(
                'callback' => 'cms_contacts_form_instrument_callback',
                'wrapper' => 'instrument-fieldset-wrapper',
            ),
        );
    }

    $form['personal_email'] = array(
        '#type' => 'textfield',
        '#title' => t('Personal email'),
        '#default_value' => isset($contact['personalemail'][0]) ? $contact['personalemail'][0] : '',
        '#size' => 30,
    );

    $form['home_phone'] = array(
        '#type' => 'textfield',
        '#title' => t('Home phone'),
        '#default_value' => isset($contact['homephone'][0]) ? $contact['homephone'][0] : '',
        '#size' => 30,
    );

    $mobile_phone_default = CMSContacts::get_multiple_attribute($contact, 'mobilephonenumbers');
    $form['mobile'] = array(
        '#type' => 'textfield',
        '#title' => t('Mobile'),
        '#default_value' => isset($mobile_phone_default) ? $mobile_phone_default : '',
        '#size' => 30,
        '#description' => t('Separate multiple entries with a comma.'),
        '#maxlength' => 512,
    );

    $form['skype'] = array(
        '#type' => 'textfield',
        '#title' => t('Skype ID'),
        '#default_value' => isset($contact['skype'][0]) ? $contact['skype'][0] : '',
        '#size' => 30,
    );

    $additional_email_default = CMSContacts::get_multiple_attribute($contact, 'additionalemails');
    $form['additional_emails'] = array(
        '#type' => 'textfield',
        '#title' => t('Additional emails'),
        '#default_value' => $additional_email_default,
        '#description' => t('Separate multiple entries with a comma'),
        '#size' => 30,
        '#maxlength' => 512,
    );

    $website_default = CMSContacts::get_multiple_attribute($contact, 'websites');
    $form['website'] = array(
        '#type' => 'textfield',
        '#title' => t('Websites'),
        '#default_value' => isset($website_default) ? $website_default : '',
        '#description' => t('Separate multiple entries with a comma'),
        '#size' => 30,
        '#maxlength' => 512,
    );

    $form['country_post'] = array(
        '#type' => 'select',
        '#title' => t('Country post'),
        '#options' => $country_options,
        '#default_value' => isset($contact['cpiso2'][0]) ? $contact['cpiso2'][0] : '',
        '#description' => t('Country where the person is stationed if it differs from the country which they represent (e.g. Ambassadors).'),
    );

    $form['organization'] = array(
        '#type' => 'textfield',
        '#title' => t('Organization'),
        '#default_value' => isset($contact['o'][0]) ? $contact['o'][0] : '',
        '#size' => 30,
        '#maxlength' => 512,
    );

    $form['department'] = array(
        '#type' => 'textfield',
        '#title' => t('Department'),
        '#default_value' => isset($contact['ou'][0]) ? $contact['ou'][0] : '',
        '#size' => 30,
        '#maxlength' => 512,
    );

    $languages = CMSUtils::vocabulary_get_terms(VOC_UN_LANGUAGES);
    $contact_language = CMSContacts::contact_map_terms($contact, 'preferredlanguages', $languages);
    $form['preferred_language'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('Preferred language'),
        '#default_value' => (!empty($contact_language)) ? $contact_language : '',
        '#options' => $languages,
    );

    $species = node_load_multiple(array(), array('type' => 'species'));
    if (isset($contact['uid'])) {
        $contact_species = CMSContacts::get_contact_related_content($contact['uid'][0], 'species', 'field_species_experts');
    }

    $species_default_value = array();
    if (isset($contact_species)) {
        foreach ($contact_species as $contact_species_data) {
            $species_default_value[] = $contact_species_data->nid;
        }
    }

    $species_options = array();
    foreach ($species as $node) {
        $species_options[$node->nid] = $node->title;
    }

    $form['species'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('Species'),
        '#default_value' => (!empty($species_default_value)) ? $species_default_value : '',
        '#options' => $species_options,
        '#size' => 5,
    );

    $meetings = node_load_multiple(array(), array('type' => 'meeting'));
    if (isset($contact['uid'])) {
        $contact_meetings = CMSContacts::get_contact_related_content($contact['uid'][0], 'meeting', 'field_meeting_participants');
    }

    $meetings_default_value = array();
    if (isset($contact_meetings)) {
        foreach ($contact_meetings as $contact_meeting_data) {
            $meetings_default_value[] = $contact_meeting_data->nid;
        }
    }

    $meetings_options = array();
    foreach ($meetings as $node) {
        $meetings_options[$node->nid] = $node->title;
    }

    $form['meetings'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('Meetings'),
        '#default_value' => (!empty($meetings_default_value)) ? $meetings_default_value : '',
        '#options' => $meetings_options,
        '#size' => 5,
    );

    $region_options[''] = t('Select region');
    $regions  = CMSUtils::vocabulary_get_terms(VOC_REGION);
    $contact_regions = CMSContacts::contact_map_terms($contact, 'region', $regions);
    $region_options = $region_options + $regions;

    $form['region'] = array(
        '#type' => 'select',
        '#title' => t('Region'),
        '#options' => $region_options,
        '#default_value' => isset($contact_regions) ? $contact_regions : '',
    );

    $form['country'] = array(
        '#type' => 'select',
        '#title' => t('Country'),
        '#options' => $country_options,
        '#default_value' => isset($contact['iso2'][0]) ? $contact['iso2'][0] : '',
    );

    $form['city'] = array(
        '#type' => 'textfield',
        '#title' => t('Town/City'),
        '#default_value' => isset($contact['st'][0]) ? $contact['st'][0] : '',
        '#size' => 30,
    );

    $form['zip_code'] = array(
        '#type' => 'textfield',
        '#title' => t('Postal code'),
        '#default_value' => isset($contact['postalcode'][0]) ? $contact['postalcode'][0] : '',
        '#size' => 6,
    );

    $form['address'] = array(
        '#type' => 'textarea',
        '#title' => t('Address'),
        '#default_value' => isset($contact['street'][0]) ? $contact['street'][0] : '',
        '#rows' => 5,
    );

    $form['description'] = array(
        '#type' => 'textarea',
        '#title' => t('Description'),
        '#default_value' => isset($contact['description'][0]) ? $contact['description'][0] : '',
        '#rows' => 5,
    );

    return $form;
}



/**
 * Implements hook_form();
 *
 * Generate the Add/Edit form for LDAP organization
 *
 * @param   array   $form
 * @param   array   $form_state
 * @param   array   $contact
 *
 * @return  array   $form
 */
function organization_form($form, &$form_state, $organization) {
    if (!isset($form_state['organization'])) {
        $form['#organization_edit_form'] = FALSE;
        $form_state['organization'] = $organization;
    }else {
        $form['#organization_edit_form'] = TRUE;
        $organization = $form_state['organization'];
    }

    $countries = countries_get_countries();
    $country_options[''] = t('Select country');
    foreach ($countries as $country) {
        $country_options[$country->iso2] = $country->name;
    }

    $form['#attributes']['class'][] = 'organization-form';
    $form['#attributes']['class'][] = 'form-horizontal';
    $form['#organization'] = $organization;

    // Add the buttons.
    $form['actions'] = array(
        '#type' => 'actions',
        '#prefix' => '<div class="form-actions">',
        '#suffix' => '</div>',
    );

    $form['actions']['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Save'),
        '#weight' => 5,
        '#submit' => array('organization_form_submit'),
    );

    $form['availability'] = array(
        '#type' => 'radios',
        '#title' => t('Availability'),
        '#default_value' => isset($organization['cmsavailability'][0]) ? $organization['cmsavailability'][0] : "TRUE",
        '#options' => array(
            "TRUE" => t('Available'),
            "FALSE" => t('Unavaiable'),
        ),
        '#label' => 'inline',
    );

    if (!empty($organization['uid'])) {
        $form['actions']['delete'] = array(
            '#type' => 'submit',
            '#value' => t('Delete'),
            '#weight' => 15,
            '#submit' => array('organization_form_delete_submit'),
        );
    }

    $form['#validate'][] = 'organization_form_validate';
    $form['name'] = array(
        '#title' => t('Name'),
        '#type' => 'textfield',
        '#default_value' => isset($organization['o'][0]) ? $organization['o'][0] : '',
        '#size' =>  30,
        '#label' => 'inline',
        '#maxlength' => 512,
    );

    $form['email'] = array(
        '#type' => 'textfield',
        '#title' => t('Work email'),
        '#default_value' => isset($organization['mail'][0]) ? $organization['mail'][0] : '',
        '#size' => 30,
    );

    $organization_statuses = CMSUtils::vocabulary_get_terms(VOC_STATUS_ORGANIZATION);
    $contact_org_statuses = CMSContacts::contact_map_terms($organization, 'statusorganization', $organization_statuses);

    $form['organization_status'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('Status organization'),
        '#default_value' => (!empty($contact_org_statuses)) ? $contact_org_statuses : '',
        '#options' => $organization_statuses,
        '#size' => 5,
    );

    $form['department'] = array(
        '#type' => 'textfield',
        '#title' => t('Department'),
        '#default_value' => isset($organization['ou'][0]) ? $organization['ou'][0] : '',
        '#size' => 30,
        '#maxlength' => 512,
    );

    $mailing_lists = CMSUtils::vocabulary_get_terms(VOC_CONTACTS_MAILING);
    $contact_mailing_lists = CMSContacts::contact_map_terms($organization, 'mailinglists', $mailing_lists);

    $form['mailing_list'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('Mailing lists'),
        '#default_value' => (!empty($contact_mailing_lists)) ? $contact_mailing_lists : '',
        '#options' => $mailing_lists,
        '#size' => 5,
    );

    $language_options[''] = t('Select language');
    $languages = CMSUtils::vocabulary_get_terms(VOC_UN_LANGUAGES);
    $contact_language = CMSContacts::contact_map_terms($organization, 'preferredlanguages', $languages);
    $language_options = $language_options + $languages;

    $form['preferred_language'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('Preferred language'),
        '#default_value' => (!empty($contact_language)) ? $contact_language : '',
        '#options' => $language_options,
    );
    $form['skype'] = array(
        '#type' => 'textfield',
        '#title' => t('Skype ID'),
        '#default_value' => isset($organization['skype'][0]) ? $organization['skype'][0] : '',
        '#size' => 30,
    );

    $additional_email_default = '';
    if (isset($organization['additionalemails']) && $organization['additionalemails']['count']) {
        foreach ($organization['additionalemails'] as $index => $email_address) {
            if (is_numeric($index)) {
                $additional_email_default .= $email_address;
                if (($index + 1) < $organization['additionalemails']['count']) {
                    $additional_email_default .= ", ";
                }
            }
        }
    }

    $form['additional_emails'] = array(
        '#type' => 'textfield',
        '#title' => t('Additional emails'),
        '#default_value' => $additional_email_default,
        '#description' => t('Separated by comma'),
        '#size' => 30,
        '#maxlength' => 512,
    );

    $website_default = CMSContacts::get_multiple_attribute($organization, 'websites');
    $form['website'] = array(
        '#type' => 'textfield',
        '#title' => t('Website'),
        '#default_value' => isset($website_default) ? $website_default : '',
        '#size' => 30,
        '#maxlength' => 512,
    );

    $form['country'] = array(
        '#type' => 'select',
        '#title' => t('Country'),
        '#options' => $country_options,
        '#default_value' => isset($organization['iso2'][0]) ? $organization['iso2'][0] : '',
    );

    $form['country_post'] = array(
        '#type' => 'select',
        '#title' => t('Country post'),
        '#options' => $country_options,
        '#default_value' => isset($organization['cpiso2'][0]) ? $organization['cpiso2'][0] : '',
    );

    $region_options[''] = t('Select region');
    $regions  = CMSUtils::vocabulary_get_terms(VOC_REGION);
    $contact_regions = CMSContacts::contact_map_terms($organization, 'region', $regions);
    $region_options = $region_options + $regions;

    $form['region'] = array(
        '#type' => 'select',
        '#title' => t('Region'),
        '#options' => $region_options,
        '#default_value' => isset($contact_regions) ? $contact_regions : '',
    );

    $form['city'] = array(
        '#type' => 'textfield',
        '#title' => t('Town/City'),
        '#default_value' => isset($organization['st'][0]) ? $organization['st'][0] : '',
        '#size' => 30,
    );

    $form['zip_code'] = array(
        '#type' => 'textfield',
        '#title' => t('Postal code'),
        '#default_value' => isset($organization['postalcode'][0]) ? $organization['postalcode'][0] : '',
        '#size' => 6,
    );

    $form['address'] = array(
        '#type' => 'textarea',
        '#title' => t('Address'),
        '#default_value' => isset($organization['street'][0]) ? $organization['street'][0] : '',
        '#rows' => 5,
    );

    $form['description'] = array(
        '#type' => 'textarea',
        '#title' => t('Description'),
        '#default_value' => isset($organization['description'][0]) ? $organization['description'][0] : '',
        '#rows' => 5,
        '#maxlength' => 512,
    );

    $form['phone'] = array(
        '#type' => 'textfield',
        '#title' => t('Phone number'),
        '#default_value' => isset($organization['workphonenumbers'][0]) ? $organization['workphonenumbers'][0] : '',
        '#size' => 30,
    );

    $mobile_phone_default = CMSContacts::get_multiple_attribute($organization, 'mobilephonenumbers');
    $form['mobile'] = array(
        '#type' => 'textfield',
        '#title' => t('Mobile'),
        '#default_value' => isset($mobile_phone_default) ? $mobile_phone_default : '',
        '#size' => 30,
        '#maxlength' => 512,
    );

    $fax_default = CMSContacts::get_multiple_attribute($organization, 'faxnumbers');
    $form['fax_number'] = array(
        '#type' => 'textfield',
        '#title' => t('Fax'),
        '#default_value' => isset($fax_default) ? $fax_default : '',
        '#size' => 30,
        '#maxlength' => 512,
    );

    $options = CMSLegalInstrument::cms_instruments_names();
    $form['conventions'] = array(
        '#type' => 'select',
        '#multiple' => TRUE,
        '#title' => t('CMS Instruments'),
        '#default_value' => isset($organization['conventions']) ? $organization['conventions'] : '',
        '#options' => $options,
    );

    return $form;
}

/**
 * Callback for both ajax-enabled buttons.
 *
 * Selects and returns the fieldset with the names in it.
 */
function cms_contacts_form_instrument_callback($form, &$form_state) {
    return $form['instrument_fieldset'];
}

/**
 * Submit handler for the "add-one-more" button.
 *
 * Increments the max counter and causes a rebuild.
 */
function cms_contacts_form_add_more_add_one_instrument($form, &$form_state) {
    $form_state['num_instrument']++;
    $form_state['rebuild'] = TRUE;

}

/**
 * Submit handler for the "remove one" button.
 *
 * Decrements the max counter and causes a form rebuild.
 */
function cms_contacts_form_remove_one_instrument($form, &$form_state) {
    if ($form_state['num_instrument'] > 1) {
        $form_state['num_instrument']--;
    }
    $form_state['rebuild'] = TRUE;
}
