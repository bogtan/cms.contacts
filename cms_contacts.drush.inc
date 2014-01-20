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

    $items['generate_nfp_structure'] = array(
        'arguments' => array(
            'destination' => 'Required'
        ),
        'required-arguments' => TRUE,
        'aliases' => array('gns'),
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

function drush_cms_contacts_generate_nfp_structure($destination_file = '') {
    $cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$users_cache, 'update_ldap_users_cache');
    $instruments_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$instruments_cache, 'update_ldap_instruments_cache');

    $fp = array(
        'cms' => 'f0b4ae05-8d04-4814-b01f-064c94da5a23',
        'aewa' => 'dbce2ddc-648e-4302-be84-47d0aa36ad66',
        'cms/aewa' => 'f0b4ae05-8d04-4814-b01f-064c94da5a23',
    );

    $ldif = "";
    $created_countries = array();
    foreach ($cache as $index => $item) {
        if (array_key_exists('statusperson', $item) && !empty($item['statusperson'])) {
            foreach ($item['statusperson'] as $index => $list_name) {
                if (strtolower($list_name) == 'focal point') {
                    $uuid = $fp[strtolower($item['conventions'][0])];

                    $country_name = CMSUtils::$country_list[$item['iso2'][0]];

                    if (!array_key_exists($item['iso2'][0], CMSUtils::$country_list)) {
                        print $item['iso2'][0];
                    }

                    $dn = "iso2={$item['iso2'][0]},uuid={$uuid}," . variable_get(LDAP_BASE_DN, '');

                    $ldif .= "
                        dn: {$dn}
                        objectClass: cmsCountryNFP
                        iso2: {$item['iso2'][0]}
                        uuid: {$uuid}
                        cn: {$country_name}
                        uniqueMember: {$item['dn']}

                    ";

                    if (!array_key_exists($dn, $created_countries)) {
                        $created_countries[] = $dn;
                    }
                }
            }
        }
    }

    foreach (CMSUtils::$country_list as $iso2 => $country_name) {
        foreach ($instruments_cache as $index => $item) {
            $dn = "iso2={$iso2},uuid={$item['uuid'][0]}," . variable_get(LDAP_BASE_DN, '');
            if (!array_key_exists($dn, $created_countries)) {
                $ldif .= "
                    dn: {$dn}
                    objectClass: cmsCountryNFP
                    iso2: {$iso2}
                    uuid: {$item['uuid'][0]}
                    cn: {$country_name}
                    uniqueMember:

                ";

                $created_countries[] = $dn;
            }
        }
    }

    $fp = fopen($destination_file . '/nfp.ldif', 'w');
    fwrite($fp, $ldif);
    fclose($fp);

    return TRUE;
}
