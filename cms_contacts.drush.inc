<?php

/**
 * Implements hook_drush_command();
 *
 * Drush commands for Contact module
 *
 * @author Bogdan Tanase
 *
 * @return   array   $items
 */
function cms_contacts_drush_command() {
    $items['update_countries_and_zipcodes'] = array(
        'description' => 'Update contacts country and zip codes details',
        'arguments' => array(
            'json_file' => 'Required',
        ),
        'required-arguments' => TRUE,
        'aliases' => array('ucz')
    );

    return $items;
}

/**
 * Callback function for update_countries_and_zipcodes drush command.
 *
 * Update contactss country and ZIP codes details based on a JSON file
 *
 * @author  Bogdan Tanase
 *
 * @param   string   $variable_name
 * @param   string   $value_to_encrypt
 */
function drush_cms_contacts_update_countries_and_zipcodes($json_file) {
    if (!file_exists($json_file)) {
        print "The path to the JSON file is not valid\n{$json_file}\n";
        return FALSE;
    }

    $contacts = json_decode(file_get_contents($json_file), TRUE);
    $contacts_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$users_cache, 'update_ldap_users_cache');

    $total_not_found = 0;
    $not_found = array();

    $total_not_saved = 0;
    $not_saved = array();

    $agent = CMSContacts::agent();
    foreach ($contacts as $index => $contact) {
        $dn = 'uid=' . $contact['uid'] . ',' . variable_get(LDAP_PEOPLE_DN, '');
        if (!array_key_exists($dn, $contacts_cache)) {
            $total_not_found ++;
            $not_found[] = "$total_not_found. ($index) {$contact['first_name']} - {$contact['last_name']}";
        }else {
            $new_contact_data = array();

            $fields = array(
                'street', 'st', 'region', 'countryEnglish', 'countryFrench', 'countrySpanish', 'iso2', 'cpIso2', 'countryPostName', 'postalCode'
            );

            foreach ($fields as $field_index => $field) {
                if (isset($contact[$field]) && !empty($contact[$field])) {
                    $new_contact_data[$field] = array($contact[$field]);
                }
            }

            if (!empty($new_contact_data)) {
                $new_contact_data['objectClass'] = array(
                    'top',
                    'cmsContact'
                );

                if(!$agent->edit($dn, $new_contact_data)) {
                    $total_not_saved ++;
                    $not_saved[] = "$total_not_saved . ($index) {$contact['first_name']} - {$contact['last_name']}";
                }
            }
        }
    }

    print "\n\n\n============ NOT FOUND ============\n\n";
    foreach ($not_found as $index => $item) {
        print $item . " not found\n";
    }

    print "\n\n\n============ NOT SAVED ============\n\n";
    foreach ($not_saved as $index => $item) {
        print $item . " could not be saved\n";
    }
}
