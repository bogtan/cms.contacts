<?php

/**
 * @file
 *    Handle listing of LDAP data, forms' validations and submissions, data export
 * @author
 *    Bogdan Tanase
 */

module_load_include('inc', 'cms_contacts', 'cms_contacts.forms');
module_load_include('inc', 'cms_contacts', 'cms_contacts.api');
module_load_include('inc', 'cms_species', 'cms_species.api');
module_load_include('inc', 'cms_meeting', 'cms_meeting.api');
module_load_include('module', 'cms_instrument', 'cms_instrument');
module_load_include('module', 'cms', 'cms');

define('AVAILABILITY_STATUSES', serialize(array("TRUE", "FALSE")));
define('DEFAULT_CONTACTS_FILTER_OPERATOR', serialize(array("is_all_of", "is_one_of")));

/**
 * @return   array
 */
function contacts_list() {
    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    drupal_set_breadcrumb($breadcrumb);
    drupal_set_title(t('Contacts'));

    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : array();
    $per_page_options = array(10, 25, 50, 100);

    $options = array('species' => array(), 'meetings' => array(), 'regions' => array(), 'mailing' => array(), 'status_organization' => array(), 'status_person' => array());
    _build_contacts_exposed_form_options($selected_instruments, $options);

    $mapped_instruments = CMSLegalInstrument::list_keyed_by_uuid();

    $users = _build_contacts_filter();
    foreach ($users as $index => $user) {
        $users[$index]['countries'] = $users[$index]['countries_post'] = $users[$index]['instruments'] = '';

        if (isset($user['iso2'])) {
            if ($user['iso2']['count'] > 1) {
                $countries = "";
                foreach ($user['iso2'] as $index => $country) {
                    if (is_numeric($index)) {
                        $country_ob .= country_load($user['iso2'][$index]);
                        $countries .= (is_object($country_ob) && property_exists($country_ob, 'name')) ? $country_ob->name : '';
                        if ($index < $user['iso2']['count']) {
                            $countries .= ", ";
                        }
                    }
                }

                $users[$index]['countries'] = $countries;
            }else {
                $country = country_load($user['iso2'][0]);
                $users[$index]['countries'] = (is_object($country) && property_exists($country, 'name')) ? $country->name : '';
            }
        }

        if (isset($user['cpiso2'])) {
            if ($user['cpiso2']['count'] > 1) {
                $countries = "";
                foreach ($user['cpiso2'] as $index => $country) {
                    if (is_numeric($index)) {
                        $countries .= country_load($user['cpiso2'][$index])->name;
                        if ($index < $user['cpiso2']['count']) {
                            $countries .= ", ";
                        }
                    }
                }

                $users[$index]['countries_post'] = $countries;
            }else {
                $country = country_load($user['cpiso2'][0])->name;
                $users[$index]['countries_post'] = $country;
            }
        }

        $main_instrument = '';
        $aaData[$index][] = (isset($user['st']) && !empty($user['st'])) ? $user['st'][0] : '';

        $contact_instruments = CMSContacts::get_groups_for_contact(CMSContacts::$instruments_cache, 'update_ldap_instruments_cache', $user['dn'], '');
        $instruments_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$instruments_cache, 'update_ldap_instruments_cache');

        $conventions = '';
        if ($contact_instruments) {
            foreach ($contact_instruments as $instrument_index => $instrument_dn) {
                if (array_key_exists($instrument_dn, $instruments_cache) && array_key_exists('cn', $instruments_cache[$instrument_dn])) {
                    $conventions .= $instruments_cache[$instrument_dn]['cn'][0];
                    if (($instrument_index + 1) < count($contact_instruments)) {
                        $conventions .= ", ";
                    }
                }
            }
        }

        $users[$index]['instruments'] = $conventions;
    }

    return theme('contacts_list', array(
        'page' => (isset($_GET['page']) && (is_numeric($_GET['page']))) ? $_GET['page'] : 1,
        'per_page' => (isset($_GET['per_page']) && (is_numeric($_GET['per_page'])) && (in_array($_GET['per_page'], $per_page_options))) ? $_GET['per_page'] : 10,
        'per_page_options' => $per_page_options,
        'operator' => (isset($_GET['operator']) && in_array($_GET['operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['operator'] : 'is_all_of',
        'per_field_operator' => (isset($_GET['per_field_operator']) && in_array($_GET['per_field_operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['per_field_operator'] : 'is_all_of',
        'instrument' => $selected_instruments,
        'countries' => isset($_GET['country']) ? $_GET['country'] : array(),
        'instruments' => $mapped_instruments,
        'regions' => isset($_GET['region']) ? $_GET['region'] : array(),
        'mailing_list' => isset($_GET['mailing_list']) ? $_GET['mailing_list'] : array(),
        'availability' => (isset($_GET['availability']) && (in_array($_GET['availability'], unserialize(AVAILABILITY_STATUSES)))) ? $_GET['availability'] : "",
        'person_status' => isset($_GET['person_status']) ? $_GET['person_status'] : array(),
        'organization_status' => isset($_GET['organization_status']) ? $_GET['organization_status'] : array(),
        'species_options' => $options["species"],
        'meeting_options' => $options["meetings"],
        'region_options' => $options["regions"],
        'mailing_options' => $options["mailing"],
        'org_status_options' => $options["status_organization"],
        'person_status_options' => $options["status_person"],
        'meeting' => isset($_GET['meeting']) ? $_GET['meeting'] : '',
        'species' => isset($_GET['species']) ? $_GET['species'] : '',
        'users' => $users,
    ));
}

/**
 * Callback function for Contacts dataTables serverside processing.
 */
function organizations_datatables_list() {
    // Map LDAP attributes for filtering
    $columns = array(
        'oid',
        'o',
        'iso2',
        'st',
        'conventions',
    );

    $organizations = array();
    $page = (isset($_GET['iDisplayStart']) && intval($_GET['iDisplayStart'])) ? $_GET['iDisplayStart'] : 1;
    $per_page = (isset($_GET['iDisplayLength']) && intval($_GET['iDisplayLength'])) ? $_GET['iDisplayLength'] : 10;
    $page = $page / $per_page + 1;
    $total_entries = 0;

    $instruments = CMSLegalInstrument::get_mapped_instruments();
    $insturments_names = array();
    foreach (array_keys($instruments) as $instrument_name) {
        $instruments_names[strtolower($instrument_name)] = $instrument_name;
    }

    // Connect to LDAP server
    $agent = CMSContacts::agent();
    if (!$agent->connected) {
        drupal_set_message('Unable to connect to the LDAP server! Please contact site administrator.', 'error');
    }else {
        $filter = _build_contacts_filter('cmsOrganization');

        // Search LDAP records based on filter
        $result = $agent->search($agent->organization_dn, $filter);

        // Sort LDAP search results
        if (isset($_GET['iSortCol_0'])) {
            $result = $agent->sort($columns[$_GET['iSortCol_0']]);
        }

        $all_organizations = $agent->get_entries();
        $total_entries = $all_organizations['count'];

        // Remove 'count' element from users results;
        // If sort direction is descending we don't want to include 'count' too
        unset($all_organizations['count']);

        if (isset($_GET['sSortDir_0']) && ($_GET['sSortDir_0'] == 'desc')) {
            $all_organizations = array_reverse($all_organizations);
        }

        // Page search results
        $paged_entries = array_chunk($all_organizations, $per_page);
        $organizations = $paged_entries[$page - 1];
        $total_pages = ceil($total_entries / $per_page);
    }

    // Build data for dataTables
    $aaData = array();
    foreach ($organizations as $index => $organization) {
        $aaData[$index] = array(
            $organization['oid'][0],
            $organization['o'][0],
        );

        if (isset($organization['iso2'])) {
            if ($organization['iso2']['count'] > 1) {
                $countries = "";
                foreach ($organization['iso2'] as $index => $country) {
                    if (is_numeric($index)) {
                        $countries .= country_load($organization['iso2'][$index])->name;
                        if ($index < $organization['iso2']['count']) {
                            $countries .= ", ";
                        }
                    }
                }

                $aaData[$index][] = $countries;
            }else {
                $country = country_load($organization['iso2'][0])->name;
                $aaData[$index][] = $country;
            }
        }else {
            $aaData[$index][] = '';
        }

        $main_instrument = '';
        $aaData[$index][] = (isset($organization['st']) && !empty($organization['st'])) ? $organization['st'][0] : '';

        if (isset($organization['conventions'])) {
            if ($organization['conventions']['count'] > 1) {
                $conventions = "";
                foreach ($organization['conventions'] as $convention_index => $convention) {
                    if ((is_numeric($convention_index)) && (isset($instruments_names[$convention]))) {
                        if ($main_instrument == '') {
                            $main_instrument = $convention;
                        }

                        $conventions .= $instruments_names[$convention];
                        if (($convention_index + 1) < $organization['conventions']['count']) {
                            $conventions .= ", ";
                        }
                    }
                }
            }else {
                $convention = $organization['conventions'][0];
                $main_instrument = $convention;
                $conventions = (isset($instruments_names[$convention])) ? $instruments_names[$convention] : '';
            }
        }

        $aaData[$index][] = $conventions;
        $aaData[$index][] = $main_instrument;
    }


    $response = array(
        'sEcho' => $_GET['sEcho'],
        'iTotalRecords' => $total_entries,
        'iTotalDisplayRecords' => $total_entries,
        'aaData' => $aaData,
    );

    echo json_encode($response);
    drupal_exit();
}


/**
 * Callback function for menu.
 */
function organizations_list() {
    $groups = CMSUtils::get_mapped_websites();
    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : array();

    $options = array('species' => array(), 'meetings' => array(), 'regions' => array(), 'mailing' => array(), 'status_organization' => array(), 'status_person' => array());
    _build_contacts_exposed_form_options($selected_instruments, $options);

    $mapped_instruments = array();
    $page = (isset($_GET['page']) && (is_numeric($_GET['page']))) ? $_GET['page'] : 1;
    $per_page_options = array(10, 25, 50, 100);
    $per_page = (isset($_GET['per_page']) && (is_numeric($_GET['per_page'])) && (in_array($_GET['per_page'], $per_page_options))) ? $_GET['per_page'] : 10;

    foreach ($groups as $group => $title) {
        $mapped_instruments[CMSUtils::slug($group)] = $title;
    }

    $mapped_instruments = CMSLegalInstrument::cms_instruments_names();

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');
    drupal_set_breadcrumb($breadcrumb);

    return theme('contacts_organizations_list', array(
                                                     'page' => $page,
                                                     'per_page' => $per_page,
                                                     'per_page_options' => $per_page_options,
                                                     'operator' => (isset($_GET['operator']) && in_array($_GET['operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['operator'] : 'is_all_of',
                                                     'per_field_operator' => (isset($_GET['per_field_operator']) && in_array($_GET['per_field_operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['per_field_operator'] : 'is_all_of',
                                                     'instrument' => $selected_instruments,
                                                     'countries' => isset($_GET['country']) ? $_GET['country'] : array(),
                                                     'instruments' => $mapped_instruments,
                                                     'regions' => isset($_GET['region']) ? $_GET['region'] : array(),
                                                     'mailing_list' => isset($_GET['mailing_list']) ? $_GET['mailing_list'] : array(),
                                                     'availability' => (isset($_GET['availability']) && (in_array($_GET['availability'], unserialize(AVAILABILITY_STATUSES)))) ? $_GET['availability'] : "",
                                                     'person_status' => isset($_GET['person_status']) ? $_GET['person_status'] : array(),
                                                     'organization_status' => isset($_GET['organization_status']) ? $_GET['organization_status'] : array(),
                                                     'species_options' => $options['species'],
                                                     'meeting_options' => $options['meetings'],
                                                     'region_options' => $options['regions'],
                                                     'mailing_options' => $options['mailing'],
                                                     'org_status_options' => $options['status_organization'],
                                                     'person_status_options' => $options['status_person'],
                                                     'meeting' => isset($_GET['meeting']) ? $_GET['meeting'] : '',
                                                     'species' => isset($_GET['species']) ? $_GET['species'] : '',
                                                ));
}
/**
 * Callback function for menu item.
 *
 * Show contact details of a specified UID
 */
function contacts_view($contact_id) {
    $contact = CMSContacts::get_contact_from_cache($contact_id);

    if (empty($contact)) {
        drupal_not_found();
        drupal_exit();
    }

    $page_title = $contact['givenname'][0] . ' ' . $contact['sn'][0];
    drupal_set_title($page_title, PASS_THROUGH);

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');
    drupal_set_breadcrumb($breadcrumb);

    $contact['conventions'] = CMSContacts::get_friendly_title_groups_for_contact(CMSContacts::$instruments_cache, 'update_ldap_instruments_cache', $contact['dn']);
    $contact['conventions']['count'] = count($contact['conventions']);

    $contact['statusperson'] = CMSContacts::get_friendly_title_groups_for_contact(CMSContacts::$status_person_cache, 'update_ldap_taxonomies_cache', $contact['dn']);
    $contact['status_person']['count'] = count($contact['statusperson']);

    $contact['statusorganization'] = CMSContacts::get_friendly_title_groups_for_contact(CMSContacts::$status_organization_cache, 'update_ldap_taxonomies_cache', $contact['dn']);
    $contact['statusorganization']['count'] = count($contact['statusorganization']);

    $contact['mailinglists'] = CMSContacts::get_friendly_title_groups_for_contact(CMSContacts::$mailing_list_cache, 'update_ldap_taxonomies_cache', $contact['dn']);
    $contact['mailinglists']['count'] = count($contact['mailinglists']);

    $contact['species'] = CMSContacts::get_contact_related_content($contact_id, 'species', 'field_species_experts');
    $contact['meetings'] = CMSContacts::get_contact_related_content($contact_id, 'meeting', 'field_meeting_participants');

    return theme('contacts_view', array(
        'user' => $contact,
    ));
}

/**
 * Callback function for menu item.
 *
 * Show organization details of a specified OID
 */
function organization_view($organization_id, $group) {
    $agent = CMSContacts::agent();

    $organization = $agent->search_organization($organization_id);
    $exists = CMSContacts::organization_in_group($group, $organization, $agent);

    if (empty($organization) || (!$exists)) {
        drupal_not_found();
        drupal_exit();
    }

    $page_title = $organization['o'][0];
    drupal_set_title($page_title);

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');
    $breadcrumb[] = l(t('Organizations'), ADMINISTRATION_PATH . 'contacts/organizations/listing');
    drupal_set_breadcrumb($breadcrumb);

    $organization['instrument'] = $group;
    return theme('contacts_organization_view', array(
                                                    'user' => $organization,
                                               ));
}

/**
 * Contacts add form validate handler
 */
function cms_contacts_form_validate($form, &$form_state) {
    $values = $form_state['values'];
    if (!empty($value['email']) && !valid_email_address($values['email'])) {
        form_set_error('email', t('Enter a valid email address.'));
    }

    if (!empty($values['personal_email']) && !valid_email_address($values['personal_email'])) {
        form_set_error('personal_email', t('Enter a valid personal email address.'));
    }

    if (!empty($values['additional_emails'])) {
        $additional_emails = explode(',', $values['additional_emails']);
        foreach ($additional_emails as $email) {
            $email = trim($email);
            if (!valid_email_address($email)) {
                form_set_error('additional_emails', t('Enter only valid additional email addresses.'));
            }
        }
    }
}

/**
 * Organization add form validate handler
 */
function organization_form_validate($form, &$form_state) {
    $values = $form_state['values'];
    if (!isset($values['name'])) {
        form_set_error('', t('Name field is missing.'));
    }

    $name = filter_xss($values['name']);
    if (empty($name)) {
        form_set_error('name', 'Organization name is required.');
    }
}

/**
 * Contacts add form submit handler
 */
function cms_contacts_form_submit($form, &$form_state) {
    $edit = (isset($form['#contact']) && (!empty($form['#contact']))) ? TRUE : FALSE;

    $site_uuid = variable_get('site_uuid', '');
    if (!$site_uuid) {
        drupal_set_message(t('Unable to determine site UUID! Please contact the site administrator'), 'error');
        $form_state['rebuild'] = TRUE;
    }else {
        $user_data = CMSContacts::prepare_user_data($form_state['values'], $edit);

        if ((isset($form['#contact']) && (!empty($form['#contact'])))) {
            $uid = $form['#contact']['uid'][0];
        }else {
            $uid = $user_data['uid'][0];
        }

        $contact_link = l(t('view'), '/'. ADMINISTRATION_PATH . 'contacts/items/' . $uid . '/view');
        $watchdog_args = array('@type' => 'Contact', '%title' => $user_data['sn'][0] . ' ' . $user_data['givenName'][0]);
        $t_args = array('@type' => 'Contact', '%title' => $user_data['sn'][0] . ' ' . $user_data['givenName'][0]);

        $agent = CMSContacts::agent();
        if ($edit) {
            // Edit form
            $user = $form['#contact'];
            if (!$agent->edit($user['dn'], $user_data)) {
                drupal_set_message(t('The contact could not be saved.'), 'error');
                $form_state['rebuild'] = TRUE;
            }else {
//            CMSContacts::set_contact_related_content($uid, $form_state['values']['species'], 'species', 'field_species_experts');
//            CMSContacts::set_contact_related_content($uid, $form_state['values']['meetings'], 'meeting', 'field_meeting_participants');
                CMSContacts::update_user_groups($agent, $uid, $site_uuid, $user_data, FALSE);
                watchdog('content', '@type: updated %title.', $watchdog_args, WATCHDOG_NOTICE, $contact_link);
                drupal_set_message(t('@type %title has been updated.', $t_args));

                // Update cache
                CMSContacts::update_ldap_users_cache();
                CMSContacts::update_ldap_instruments_cache();
                CMSContacts::update_ldap_taxonomies_cache();

                // Redirect to contact page
                $semaphore = variable_get('cron_semaphore', FALSE);

                if (!$semaphore) {
                    drupal_goto('/' . ADMINISTRATION_PATH . 'contacts/item/' . $uid . '/view');
                }
            }
        }else {
            // Add form
            $people_dn = variable_get(LDAP_PEOPLE_DN, '');
            if (!$people_dn) {
                drupal_set_message(t('Invalid DN for contact.'), 'error');
                $form_state['rebuild'] = TRUE;
            }else {
                $dn = "uid=" . $uid . "," . $people_dn;
                CMSContacts::update_user_groups($agent, $uid, $site_uuid, $user_data, TRUE);
                if(!$agent->add($dn, $user_data)) {
                    drupal_set_message(t('The contact could not be saved.'), 'error');
                    $form_state['rebuild'] = TRUE;
                }else {
//                    CMSContacts::set_contact_related_content($uid, $form_state['values']['species'], 'species', 'field_species_experts');
                    watchdog('content', '@type: added %title.', $watchdog_args, WATCHDOG_NOTICE, $contact_link);
                    drupal_set_message(t('@type %title has been created.', $t_args));

                    CMSContacts::update_user_groups($agent, $uid, $site_uuid, $user_data, TRUE);

                    // Update cache
                    CMSContacts::update_ldap_users_cache();
                    CMSContacts::update_ldap_instruments_cache();
                    CMSContacts::update_ldap_taxonomies_cache();

                    // Redirect to contact page
                    $semaphore = variable_get('cron_semaphore', FALSE);

                    if (!$semaphore) {
                        drupal_goto('/' . ADMINISTRATION_PATH . 'contacts/item/' . $uid . '/view');
                    }
                }
            }
        }

        if ($uid) {
            $form_state['values']['uid'] = $uid;
            $form_state['uid'] = $uid;
            $form_state['redirect'] = '/' . ADMINISTRATION_PATH . 'contacts/listing';
        }else {
            drupal_set_message(t('An error occurred while saving the contact. Please try again or contact the site administrator.'), 'error');
        }
    }
}

/**
 * Contacts add form submit handler
 */
function organization_form_submit($form, &$form_state) {
    $edit = (isset($form['#organization']) && (!empty($form['#organization']))) ? TRUE : FALSE;
    $organization_data = CMSContacts::prepare_organization_data($form_state['values'], $edit);

    if ((isset($form['#organization']) && (!empty($form['#organization'])))) {
        $oid = $form['#organization']['oid'][0];
    }else {
        $oid = $organization_data['oid'];
    }

    $organization_link = l(t('view'), 'contacts/organization/' . $oid . '/view');
    $watchdog_args = array('@type' => 'Organization', '%title' => $organization_data['o']);
    $t_args = array('@type' => 'Organization', '%title' => $organization_data['o']);

    $agent = CMSContacts::agent();
    if ($edit) {
        // Edit form
        $organization = $form['#organization'];
        if (!$agent->edit($organization['dn'], $organization_data)) {
            drupal_set_message(t('The organization could not be saved.'), 'error');
            $form_state['rebuild'] = TRUE;
        }else {
            watchdog('content', '@type: updated %title.', $watchdog_args, WATCHDOG_NOTICE, $organization_link);
            drupal_set_message(t('@type %title has been updated.', $t_args));

            // Redirect to organization page
            $semaphore = variable_get('cron_semaphore', FALSE);

            if (!$semaphore) {
                drupal_goto('/' . ADMINISTRATION_PATH . 'contacts/organization/' . $oid . '/' . $organization_data['conventions'][0] . '/view');
            }
        }
    }else {
        // Add form
        $dn = "oid=" . $oid . "," . $agent->organization_dn;
        if(!$agent->add($dn, $organization_data)) {
            drupal_set_message(t('The organization could not be saved.'), 'error');
            $form_state['rebuild'] = TRUE;
        }else {
            watchdog('content', '@type: added %title.', $watchdog_args, WATCHDOG_NOTICE, $organization_link);
            drupal_set_message(t('@type %title has been created.', $t_args));

            // Redirect to organization page
            $semaphore = variable_get('cron_semaphore', FALSE);

            if (!$semaphore) {
                drupal_goto('/' . ADMINISTRATION_PATH . 'contacts/organization/' . $oid . '/' . $organization_data['conventions'][0] . '/view');
            }
        }
    }

    if ($oid) {
        $form_state['values']['oid'] = $oid;
        $form_state['oid'] = $oid;
        $form_state['redirect'] = '/' . ADMINISTRATION_PATH . 'contacts/organizations/listing';
    }else {
        drupal_set_message(t('The organization could not be saved.'), 'error');
    }

    cache_clear_all();
}

/**
 * Contacts add form
 */
function contacts_add($type) {
    drupal_set_title(t('Add new @name', array('@name' => $type)), PASS_THROUGH);

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');

    drupal_set_breadcrumb($breadcrumb);

    switch($type) {
        case 'contact':
            return drupal_get_form('cms_contacts_form', array());
        default;
            break;

        case 'organization':
            return drupal_get_form('organization_form', array());
            break;
    }
}

/**
 * Contacts edit form
 */
function contacts_edit($contact_id) {
    $contact = CMSContacts::get_contact_from_cache($contact_id);

    if (empty($contact)) {
        drupal_not_found();
        drupal_exit();
    }

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');
    $breadcrumb[] = l($contact['givenname'][0] . ' ' . $contact['sn'][0], ADMINISTRATION_PATH . 'contacts/item/' . $contact_id . '/view');

    drupal_set_breadcrumb($breadcrumb);
    $page_title = $contact['givenname'][0] . ' ' . $contact['sn'][0];
    drupal_set_title(t('<em>Edit @type</em> @title', array('@type' => 'Contact', '@title' => $page_title)), PASS_THROUGH);
    return drupal_get_form('cms_contacts_form', $contact);
}



/**
 * Organizations edit form
 */
function organization_edit($organization_id, $group) {
    $current_profile = CMSUtils::get_current_profile();
    if ($current_profile != 'cms') {
        $group = $current_profile;
    }

    $agent = CMSContacts::agent();
    $organization = $agent->search_organization($organization_id);
    $exists = CMSContacts::organization_in_group($group, $organization, $agent);

    if (empty($organization) || (!$exists)) {
        drupal_not_found();
        drupal_exit();
    }

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');
    $breadcrumb[] = l(t('Organizations'), ADMINISTRATION_PATH . 'contacts/organizations/listing');
    $breadcrumb[] = l(CMSUtils::character_limiter($organization['o'][0], 100), ADMINISTRATION_PATH . 'contacts/organization/' . $organization_id . '/' . $group . '/view');

    drupal_set_breadcrumb($breadcrumb);
    $page_title = $organization['o'][0];
    drupal_set_title(t('<em>Edit @type</em> @title', array('@type' => 'Organization', '@title' => $page_title)), PASS_THROUGH);
    return drupal_get_form('organization_form', $organization);
}

/**
 * Menu callback -- ask for confirmation of contact deletion
 */
function contacts_delete_confirm($form, &$form_state, $contact_id, $group) {
    $contact = CMSContacts::get_contact_from_cache($contact_id);

    if (empty($contact)) {
        drupal_not_found();
        drupal_exit();
    }

    $full_name = $contact['givenname'][0] . ' ' . $contact['sn'][0];
    $form['#user'] = $contact;
    $form['#group'] = $group;
    $form['user_id'] = array('#type' => 'value', '#value' => $contact_id);

    return confirm_form($form,
        t('Are you sure you want to delete %user contact?', array('%user' => $full_name)),
        ADMINISTRATION_PATH . 'contacts/item/' . $contact_id . '/view',
        t('This action cannot be undone.'),
        t('Delete'),
        t('Cancel')
    );
}

/**
 * Menu callback -- ask for confirmation of organization deletion
 */
function organization_delete_confirm($form, &$form_state, $organization_id, $group) {
    $current_profile = CMSUtils::get_current_profile();
    if ($current_profile != 'cms') {
        $group = $current_profile;
    }

    $agent = CMSContacts::agent();
    $organization = $agent->search_organization($organization_id);
    $exists = CMSContacts::organization_in_group($group, $organization, $agent);

    if (empty($organization) || (!$exists)) {
        drupal_not_found();
        drupal_exit();
    }

    $organization_name = $organization['o'][0];
    $form['#organization'] = $organization;
    $form['#group'] = $group;
    $form['organization_id'] = array('#type' => 'value', '#value' => $organization_id);

    return confirm_form($form,
        t('Are you sure you want to delete %organization organization?', array('%organization' => $organization_name)),
        'contacts/organization/' . $organization_id . '/' . $group . '/view',
        t('This action cannot be undone.'),
        t('Delete'),
        t('Cancel')
    );
}

/**
 * Execute node deletion
 */
function contacts_delete_confirm_submit($form, &$form_state) {
    if ($form_state['values']['confirm']) {
        $contact_id = $form['user_id']['#value'];
        $site_uuid = variable_get('site_uuid', '');
        $contact = CMSContacts::get_contact_from_cache($contact_id);

        if (!$site_uuid) {
            drupal_set_message('Unable to determine site UUID! Please contact site administrator.', 'error');
            drupal_exit();
        }

        if (empty($contact)) {
            drupal_not_found();
            drupal_exit();
        }

        $full_name = $contact['givenname'][0] . ' ' . $full_name = $contact['sn'][0];

        $dn = "uid=$contact_id," . variable_get(LDAP_PEOPLE_DN, '');

        $agent = CMSContacts::agent();
        CMSContacts::remove_contact_from_groups($agent, $dn, $site_uuid);

        $agent->delete($dn);

        CMSContacts::update_ldap_users_cache();
        CMSContacts::update_ldap_instruments_cache();
        CMSContacts::update_ldap_taxonomies_cache();

        watchdog('content', '@type: deleted %full_name.', array('@type' => 'Contact', '%full_name' => $full_name));
        drupal_set_message(t('@type %full_name has been deleted.', array('@type' => 'Contact', '%full_name' => $full_name)));
    }

    $form_state['redirect'] = '/' . ADMINISTRATION_PATH . 'contacts/listing';
}

/**
 * Execute organization deletion
 */
function organization_delete_confirm_submit($form, &$form_state) {
    if ($form_state['values']['confirm']) {
        $organization_id = $form['organization_id']['#value'];
        $group = $form['#group'];
        $current_profile = CMSUtils::get_current_profile();
        if ($current_profile != 'cms') {
            $group = $current_profile;
        }

        $agent = CMSContacts::agent();
        $organization = $agent->search_organization($organization_id);
        $exists = CMSContacts::organization_in_group($group, $organization, $agent);

        if (empty($organization) || (!$exists)) {
            drupal_not_found();
            drupal_exit();
        }

        $organization_name = $organization['o'][0];
        $dn = "oid=$organization_id," . $agent->organization_dn;
        $agent->delete($dn);

        watchdog('content', '@type: deleted %organization.', array('@type' => 'Organization', '%organization' => $organization_name));
        drupal_set_message(t('@type %organization has been deleted.', array('@type' => 'Organization', '%organization' => $organization_name)));
    }

    $form_state['redirect'] = '/' . ADMINISTRATION_PATH . 'contacts/organizations/listing';
}

/**
 * Callback function for menu item.
 *
 * Read organization LDIFs and check if a user alreardy exists and update its conventions attribute
 */
function contacts_update_duplicates() {
    $profiles = array('aewa', 'cms', 'ascobans', 'eurobats');

    $agent = CMSContacts::agent();
    foreach ($profiles as $profile) {
        $users = load_users($profile);
        foreach ($profiles as $filter_profile) {
            if ($filter_profile != $profile) {
                foreach ($users as $user) {
                    $filter = "(&(conventions=" . $filter_profile . ")(uid=" . $user->uid . "))";
                    $agent->search($agent->base_dn, $filter);
                    $results = $agent->get_entries();
                    if ($results['count'] == 1) {
                        // Check if user already has assigned the instrument
                        $exists = (in_array($profile, array_values($results[0]['conventions']))) ? TRUE : FALSE;
                        if (!$exists) {
                            $user_dn = "uid=" . $user->uid . ",ou=Users,o=CMS";
                            $user_conventions = get_conventions($results);
                            array_push($user_conventions, $profile);
                            $agent->edit($user_dn, array('conventions' => $user_conventions));
                            CMSContacts::update_user_groups($user->uid, $user_conventions, $agent);
                        }
                    }
                }
            }
        }
    }

    drupal_set_message(t('Contacts succesfully updated!'));
    $semaphore = variable_get('cron_semaphore', FALSE);

    if (!$semaphore) {
        drupal_goto('/' . ADMINISTRATION_PATH . 'contacts/listing');
    }
}

function load_users($profile = '') {
    $initial_data_path = drupal_get_path('module', 'cms_initial_data');
    $json_file_path = $initial_data_path . "/$profile/contacts/users.json";
    $file_data = file_get_contents($json_file_path);

    return json_decode($file_data);
}

function get_conventions($user) {
    $conventions = array();

    if (isset($user[0]['conventions']) && ($user[0]['conventions']['count'] > 0)) {
        unset($user[0]['conventions']['count']);
        $conventions = array_values($user[0]['conventions']);
    }

    return $conventions;
}

function build_terms_options($vocabularies = array(), $existing_options = array(), $vocabulary_name, $profile = '', $site_name = '', $show_site_name = TRUE) {
    $options = array();

    if ($vocabularies && array_key_exists($vocabulary_name, $vocabularies)) {
        foreach ($vocabularies[$vocabulary_name] as $key => $term) {
            if ($show_site_name){
                $options[$profile . ':' . $key] = $term . ' (' . $site_name . ')';
            }else {
                $options[$profile . ':' . $key] = $term;
            }
        }
    }

    if (!empty($existing_options)) {
        $options = array_merge($options, $existing_options);
    }

    return $options;
}

/**
 * Update exposed form options with selected instruments related options
 *
 * @param    string    $instrument_uuid
 * @param    array     $options
 * @param    array     $mailing_list
 * @param    array     $status_person
 * @param    array     $status_organization
*/
function _update_options($instrument_uuid, &$options, $mailing_list, $status_person, $status_organization) {
    foreach ($mailing_list as $list_dn => $list) {
        if ($list['uuid'][0] == $instrument_uuid) {
            $options['mailing'][$list_dn] = $list['friendly_title'];
        }
    }

    foreach ($status_person as $list_dn => $list) {
        if ($list['uuid'][0] == $instrument_uuid) {
            $options['status_person'][$list_dn] = $list['friendly_title'];
        }
    }

    foreach ($status_organization as $status_dn => $status) {
        if ($status['uuid'][0] == $instrument_uuid) {
            $options['status_organization'][$status_dn] = $status['friendly_title'];
        }
    }
}

/**
 *
 * @param   array   $options
 */
function _build_contacts_exposed_form_options($selected_instruments, &$options) {
    $instruments = CMSLegalInstrument::list_keyed_by_uuid();

    $mailing_list = $status_person = $status_organization = array();
    foreach ($instruments as $instrument_uuid => $instrument_title) {
        $mailing_list[$instrument_uuid] = array();
        $status_person[$instrument_uuid] = array();
        $status_organization[$instrument_uuid] = array();
    }

    if (!($mailing_list_cache = cache_get(CMSContacts::$mailing_list_cache)) ||
        !($status_person_cache = cache_get(CMSContacts::$status_person_cache)) ||
        !($status_organization_cache = cache_get(CMSContacts::$status_organization_cache))) {
        CMSContacts::update_ldap_taxonomies_cache();
        $mailing_list = cache_get(CMSContacts::$mailing_list_cache)->data;
        $status_person = cache_get(CMSContacts::$status_person_cache)->data;
        $status_organization = cache_get(CMSContacts::$status_organization_cache)->data;
    }else {
        $mailing_list = $mailing_list_cache->data;
        $status_person = $status_person_cache->data;
        $status_organization = $status_organization_cache->data;
    }

    if (empty($selected_instruments) || ($selected_instruments == 'null') || ($selected_instruments[0] == '')) {
        foreach (array_keys($instruments) as $instrument_uuid) {
            _update_options($instrument_uuid, $options, $mailing_list, $status_person, $status_organization);
        }
    }else {
        foreach ($selected_instruments as $instrument_uuid) {
            _update_options($instrument_uuid, $options, $mailing_list, $status_person, $status_organization);
        }
    }
}


/**
 * Build LDAP filter for Contacts
 *
 * @param   string   $profile   Convention to look for
 * @return  string   $filter
 */
function _build_contacts_filter($objectClass = 'cmsContact') {
//    $time_start = microtime(true);
    $selected_operator = (isset($_GET['operator']) && in_array($_GET['operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['operator'] : 'is_all_of';
    $selected_per_field_operator = (isset($_GET['per_field_operator']) && in_array($_GET['per_field_operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['per_field_operator'] : 'is_all_of';
    $countries = isset($_GET['country']) ? $_GET['country'] : '';
    $regions = isset($_GET['region']) ? $_GET['region'] : '';
    $mailing_lists = isset($_GET['mailing_list']) ? $_GET['mailing_list'] : array();
    $availability = (isset($_GET['availability']) && (in_array($_GET['availability'], unserialize(AVAILABILITY_STATUSES)))) ? $_GET['availability'] : "";
    $person_statuses = isset($_GET['person_status']) ? $_GET['person_status'] : array();
    $organization_statuses = isset($_GET['organization_status']) ? $_GET['organization_status'] : array();
    $species = isset($_GET['species']) ? $_GET['species'] : array();
    $meeting = isset($_GET['meeting']) ? $_GET['meeting'] : array();
    $search_query = (isset($_GET['sSearch']) && !empty($_GET['sSearch'])) ? $_GET['sSearch'] : '';
    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : array();

    $species_options = $meeting_options = $region_options = $mailing_options = $org_status_options = $person_status_options = array();
    $options = array('species' => array(),
                     'meetings' => array(),
                     'regions' => array(),
                     'mailing' => array(),
                     'status_organization' => array(),
                     'status_person' => array()
    );
    _build_contacts_exposed_form_options($selected_instruments, $options);

    $operator = ($selected_operator == 'is_all_of') ? '&' : '|';
    $per_field_operator = ($selected_per_field_operator == 'is_all_of') ? '&' : '|';

    $instruments_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$instruments_cache, 'update_ldap_instruments_cache');
    $users_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$users_cache, 'update_ldap_users_cache');
    $mailing_lists_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$mailing_list_cache, 'update_ldap_taxonomies_cache');
    $status_person_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$status_person_cache, 'update_ldap_taxonomies_cache');
    $status_organization_cache = CMSContacts::get_or_create_ldap_cache(CMSContacts::$status_organization_cache, 'update_ldap_taxonomies_cache');
    $users = array();

    $instruments_members = array();
    if ((!empty($selected_instruments)) && ($selected_instruments != 'null') && ($selected_instruments != array(''))) {
        foreach ($selected_instruments as $index => $instrument_uuid) {
            $key = "uuid=$instrument_uuid,o=CMS";
            $members = $instruments_cache[$key]['uniquemember'];
            CMSUtils::unset_count_key($members);
            $instruments_members = array_merge($instruments_members, $members);
        }
    }else {
        foreach ($instruments_cache as $instrument_dn => $instrument) {
            $members = $instrument['uniquemember'];
            CMSUtils::unset_count_key($members);
            $instruments_members = array_merge($instruments_members, $members);
        }
    }
    $instruments_members = array_unique($instruments_members);

    $mailing_lists_members = array();
    if (!empty($mailing_lists) && ($mailing_lists != 'null')) {
        foreach($mailing_lists as $index => $mailing_list_dn) {
            if (array_key_exists($mailing_list_dn, $mailing_lists_cache)) {
                $members = $mailing_lists_cache[$mailing_list_dn]['uniquemember'];
                CMSUtils::unset_count_key($members);
                $mailing_lists_members = array_merge($mailing_lists_members, $members);
            }
        }
    }else {
        foreach ($mailing_lists_cache as $mailing_list_dn => $mailing_list) {
            $members = $mailing_list['uniquemember'];
            CMSUtils::unset_count_key($members);
            $mailing_lists_members = array_merge($mailing_lists_members, $members);
        }
    }
    $mailing_lists_members = array_unique($mailing_lists_members);

    $status_person_members = array();
    if (!empty($person_statuses) && ($person_statuses!= 'null')) {
        // Get users from selected mailing lists
        foreach($person_statuses as $index => $status_person_dn) {
            if (array_key_exists($status_person_dn, $status_person_cache)) {
                $members = $status_person_cache[$status_person_dn]['uniquemember'];
                CMSUtils::unset_count_key($members);
                $status_person_members = array_merge($status_person_members, $members);
            }
        }
    }else {
        foreach ($status_person_cache as $status_person_dn => $status_person) {
            $members = $status_person['uniquemember'];
            CMSUtils::unset_count_key($members);
            $status_person_members = array_merge($status_person_members, $members);
        }
    }
    $status_person_members = array_unique($status_person_members);

    $status_organization_members = array();
    if (!empty($organization_statuses) && ($organization_statuses != 'null')) {
        // Get users from selected mailing lists
        foreach($organization_statuses as $index => $organization_status_dn) {
            if (array_key_exists($organization_status_dn, $status_organization_cache)) {
                $members = $status_organization_cache[$organization_status_dn]['uniquemember'];
                CMSUtils::unset_count_key($members);
                $status_organization_members = array_merge($status_organization_members, $members);
            }
        }
    }else {
        foreach ($status_organization_cache as $status_person_dn => $status_organization) {
            $members = $status_organization['uniquemember'];
            CMSUtils::unset_count_key($members);
            $status_organization_members = array_merge($status_organization_members, $members);
        }
    }
    $status_organization_members = array_unique($status_organization_members);

    $users_dns = $instruments_members;
    if (!empty($mailing_lists_members)) {
        $users_dns = array_filter(array_intersect($users_dns, $mailing_lists_members));
    }

    if (!empty($status_person_members)) {
        $users_dns = array_filter(array_intersect($users_dns, $status_person_members));
    }

    if (!empty($status_organization_members)) {
        $users_dns = array_filter(array_intersect($users_dns, $status_organization_members));
    }

    foreach ($users_dns as $index => $user_dn) {
        if (array_key_exists($user_dn, $users_cache)) {
            $users[] = $users_cache[$user_dn];
        }
    }

//    $time_end = microtime(true);
//    $execution_time = ($time_end - $time_start);
//    echo '<b>Total Execution Time:</b> ' . $execution_time . ' Seconds';

//    $users['count'] = count($users);
    return $users;
//    foreach ($instruments_members as $member_dn) {
//        echo "<pre>";
//        var_dump($member_dn);
//        echo "</pre>";
//        die();
//    }
//
//    foreach ($cache_lists as $list) {
//        CMSUtils::unset_count_key(array_filter($list));
//        $members = $list['uniquemember'];
//
//        foreach ($users as $user_dn => $user) {
//            if (!in_array($user, $members)) {
//                unset($users[$user_dn]);
//            }
//        }
//    }
//
//    if (!empty($regions) || (!empty($countries) && ($countries != 'null')) || !empty($search_query) || !empty($availability) || (!empty($mailing_lists) && ($mailing_lists != 'null')) ||
//        !empty($organization_statuses) || !empty($person_statuses) || (!empty($species) && ($species != 'null'))) {
//        $filter = "(" . $operator . $filter;
//
//        if (!empty($countries) && ($countries != 'null')) {
//            $countries = explode(',', $countries);
//            $filter .= "(" . $per_field_operator;
//            foreach($countries as $country) {
//                $filter .= "(|(iso2=" . strtolower($country) . ")(cpIso2=" . strtolower($country) . "))";
//            }
//            $filter .= ")";
//        }
//
//        if (!empty($regions) && ($regions != 'null')) {
//            $regions = explode(',', $regions);
//            $filter .= "(" . $per_field_operator;
//            foreach ($regions as $region) {
//                if (array_key_exists($region, $region_options)) {
//                    $filter .= "(region=" . $region_options[$region] . ")";
//                }
//            }
//            $filter .= ")";
//        }
//
//        if (!empty($availability)) {
//            $filter .= "(cmsAvailability=" . $availability . ")";
//        }
//
//        if (!empty($mailing_lists) && ($mailing_lists != 'null')) {
//            $mailing_lists = explode(',', $mailing_lists);
//            $filter .= "(" . $per_field_operator;
//            foreach ($mailing_lists as $mailing_list) {
//                if (array_key_exists($mailing_list, $mailing_options)) {
//                    $filter .= "(mailingLists=" . $mailing_options[$mailing_list] . ")";
//                }
//            }
//            $filter .= ")";
//        }
//
//        if (!empty($person_statuses) && ($person_statuses != 'null')) {
//            $person_statuses = explode(',', $person_statuses);
//            $filter .= "(" . $per_field_operator;
//            foreach ($person_statuses as $person_status) {
//                if (array_key_exists($person_status, $person_status_options)) {
//                    $person_status = preg_replace('/([\(\)])/',"\\\\$1", $person_status_options[$person_status]);
//                    $filter .= "(statusPerson=" . $person_status . ")";
//                }
//            }
//            $filter .= ")";
//        }
//
//        if (!empty($organization_statuses) && ($organization_statuses != 'null')) {
//            $organization_statuses = explode(',', $organization_statuses);
//            $filter .= "(" . $per_field_operator;
//            foreach ($organization_statuses as $organization_status) {
//                if (array_key_exists($organization_status, $org_status_options)) {
//                    $organization_status = preg_replace('/([\(\)])/',"\\\\$1", $org_status_options[$organization_status]);
//                    $filter .= "(statusOrganization=" . $organization_status . ")";
//                }
//            }
//
//            $filter .= ")";
//        }
//
//        if (array_key_exists($species, $species_options)) {
//            $experts = $species_options[$species]->experts_dns;
//            if (!empty($experts)) {
//                $filter .= "(" . $per_field_operator;
//                foreach ($experts as $index => $expert) {
//                    $filter .= "(uid=" . $expert . ")";
//                }
//                $filter .= ")";
//            }
//        }
//
//        if (!empty($search_query)) {
//            $filter .= "(|(sn=*" . $search_query . "*)(givenName=*" . $search_query . "*)(o=*" . $search_query . "*)(ou=*" . $search_query . "*)(st=*" . $search_query . "*)(mail=*" . $search_query . "*)(additionalEmails=*" . $search_query . "*)(personalEmail=*" . $search_query . "*)(website=*" . $search_query . "*))";
//        }
//
//        $filter .= ")";
//    }
//
//    $filter = "(&(objectClass=" . $objectClass . ")" . $filter . ")";
//    return $filter;
}

function _prepare_contact_details(&$data, $user) {
    CMSContacts::prepare_user_to_csv($data, $user, 'cmsavailability');
    CMSContacts::prepare_user_to_csv($data, $user, 'personaltitle');
    CMSContacts::prepare_user_to_csv($data, $user, 'givenname');
    CMSContacts::prepare_user_to_csv($data, $user, 'sn');
    CMSContacts::prepare_user_to_csv($data, $user, 'title');
    CMSContacts::prepare_user_to_csv($data, $user, 'personstatus');
    CMSContacts::prepare_user_to_csv($data, $user, 'o');
    CMSContacts::prepare_user_to_csv($data, $user, 'ou');
    CMSContacts::prepare_user_to_csv($data, $user, 'organizationstatus');
    CMSContacts::prepare_user_to_csv($data, $user, 'website');
    CMSContacts::prepare_user_to_csv($data, $user, 'street');
    CMSContacts::prepare_user_to_csv($data, $user, 'st');
    CMSContacts::prepare_user_to_csv($data, $user, 'postalcode');
    CMSContacts::prepare_user_to_csv($data, $user, 'iso2');
    CMSContacts::prepare_user_to_csv($data, $user, 'region');
    CMSContacts::prepare_user_to_csv($data, $user, 'cpiso2');
    CMSContacts::prepare_user_to_csv($data, $user, 'workphone');
    CMSContacts::prepare_user_to_csv($data, $user, 'mobile');
    CMSContacts::prepare_user_to_csv($data, $user, 'facsimiletelephonenumber');
    CMSContacts::prepare_user_to_csv($data, $user, 'mail');
    CMSContacts::prepare_user_to_csv($data, $user, 'additionalemails');
    CMSContacts::prepare_user_to_csv($data, $user, 'homephone');
    CMSContacts::prepare_user_to_csv($data, $user, 'skype');
    CMSContacts::prepare_user_to_csv($data, $user, 'conventions');
    CMSContacts::prepare_user_to_csv($data, $user, 'cmsmailing');
    CMSContacts::prepare_user_to_csv($data, $user, 'description');
    CMSContacts::prepare_user_to_csv($data, $user, 'preferredlanguage');

    $meetings = CMSContacts::get_contact_related_content($user['uid'][0], 'meeting', 'field_meeting_participants', TRUE);
    if (!empty($meetings)) {
        $data[] = implode('; ', $meetings);
    }else {
        $data[] = '';
    }

    $species = CMSContacts::get_contact_related_content($user['uid'][0], 'species', 'field_species_experts', TRUE);
    if (!empty($species)) {
        $data[] = implode('; ', $species);
    }else {
        $data[] = '';
    }
}


/**
 * Add contacts to groups
 */
function cms_contacts_update_groups() {
    $time_start = microtime(true);
    $contacts = array();
    $instruments_mapping = array(
        'marine turtles iosea' => 'IOSEA Marine Turtles',
        'birds of prey (raptors)' => 'Raptors (Birds of Prey)',
        'gorillas' => 'Gorilla Agreement',
        'pacific island cetaceans' => 'Pacific Islands Cetaceans',
        'great bustard' => 'Middle-European Great Bustard',
        'west african elephant' => 'West African Elephants',
        'marine turtles africa' => 'Western African Aquatic Mammals',
    );

    if($cached = cache_get('ldap_contacts', 'cache'))  {
        $contacts = $cached->data;
    }
    if(empty($contacts)) {
        $agent = CMSContacts::agent();
        $result = $agent->search($agent->people_dn, "(objectClass=cmsContact)");
        $entries = $agent->get_entries();

        if (is_array($entries) && array_key_exists('count', $entries)) {
            unset($entries['count']);

            foreach ($entries as $index => $entry) {
                $contacts[] = $entry;
            }

            cache_set('ldap_contacts', $contacts, 'cache', 60 * 60);
        }
    }

    $unidentified_instruments = array();
    foreach ($contacts as $contact) {
        if (isset($contact['conventions'])) {
            $conventions = $contact['conventions'];
            unset($conventions['count']);
            foreach ($conventions as $index => $convention) {
                if (strpos($convention, '/') !== FALSE) {
                    $parts = preg_split('/\//', $convention);
                    unset($conventions[$index]);
                    foreach ($parts as $part) {
                        $conventions[] = $part;
                    }
                }
            }

            foreach ($conventions as $convention) {
                if (array_key_exists($convention, $instruments_mapping)) {
                    $convention = $instruments_mapping[$convention];
                }

                $instrument = CMSLegalInstrument::get_by_name($convention);
                if (!$instrument && !in_array($convention, $unidentified_instruments)) {
                    $unidentified_instruments[] = $convention;
                }else {
                    $uuid = $instrument->uuid;
                }

                $filter = "(&(objectClass=cmsInstrument)(uuid=$uuid))";
                if (!isset($agent)) {
                    $agent = CMSContacts::agent();
                }

                $result = $agent->search($agent->base_dn, $filter);
                $entries = $agent->get_entries();
                if ($entries && array_key_exists('count', $entries)) {
                    $agent->add_to_group($entries[0]['dn'], array('uniqueMember' =>  $contact['dn']));
                }
            }
        }
    }

    if ($unidentified_instruments) {
        echo "Unidentifed instruments: <br />";
        echo "<pre>";
        var_dump($unidentified_instruments);
        echo "</pre>";
    }

    $time_end = microtime(true);
    $execution_time = ($time_end - $time_start);
    echo '<b>Total Execution Time:</b> '.$execution_time.' seconds';
}

/**
 * Generate LDIF for initial groups
 */
function cms_contacts_generate_initial_ldif() {
    $default_instruments = array(
        'CMS' => 'f0b4ae05-8d04-4814-b01f-064c94da5a23',
        'AEWA' => 'dbce2ddc-648e-4302-be84-47d0aa36ad66',
        'EUROBATS' => '99a489ec-64ff-4b0e-940b-d489d0b8e99a',
        'ASCOBANS' => '0b41fac4-6198-439a-a335-f7405f352681',
    );

    $profile = CMSUtils::get_current_profile();
    $websites = CMSUtils::get_all_websites();
    if (!array_key_exists($profile, $websites) || !array_key_exists('title', $websites[$profile])) {
        echo "Unknown instrument title";
        drupal_exit();
    }
    $instrument_title = $websites[$profile]['title'];

    $instruments = node_load_multiple(array(), array('type' => CMSLegalInstrument::$bundle));
    foreach ($instruments as $instrument) {
        $ldif = "
            dn: uuid={$instrument->uuid},o=CMS<br />
            objectClass: top<br />
            objectClass: cmsInstrument<br />
            uuid: {$instrument->uuid}<br />
            cn: {$instrument->title}<br />
            description: {$instrument->field_instrument_name[$instrument->language][0]['value']}<br />
            uniqueMember:<br /><br />
            dn: ou=Mailing List,uuid={$instrument->uuid},o=CMS<br />
            objectClass: top<br />
            objectClass: organizationalUnit<br />
            ou: Mailing List<br /><br />
            dn: ou=Status Person,uuid={$instrument->uuid},o=CMS<br />
            objectClass: top<br />
            objectClass: organizationalUnit<br />
            ou: Status Person<br /><br />
            dn: ou=Status Organization,uuid={$instrument->uuid},o=CMS<br />
            objectClass: top<br />
            objectClass: organizationalUnit<br />
            ou: Status Organization<br /><br />
        ";
//        echo $ldif;
    }

    $vocabularies = array(
        CMS_MAILING => VOC_CONTACTS_MAILING,
        CMS_STATUS_ORGANIZATION => VOC_STATUS_ORGANIZATION,
        CMS_STATUS_PERSON => VOC_STATUS_PERSON,
    );

    foreach ($vocabularies as $type => $vocabulary) {
        $ou = "";
        switch($type) {
            case CMS_MAILING:
                $ou = "Mailing List";
                break;
            case CMS_STATUS_ORGANIZATION:
                $ou = "Status Organization";
                break;
            case CMS_STATUS_PERSON:
                $ou = "Status Person";
                break;
        }

        $vocabulary = taxonomy_vocabulary_machine_name_load($vocabulary);
        if ($vocabulary) {
            $terms = taxonomy_get_tree($vocabulary->vid);
            if ($terms) {
                foreach ($terms as $index => $term) {

                    $ldif = "
                        dn: cn={$term->name},ou=$ou,uuid={$default_instruments[$instrument_title]},o=CMS<br />
                        objectClass: top<br />
                        objectClass: {$type}<br />
                        cn: {$term->name}<br />
                        instrumentName: {$instrument_title}<br />
                        uuid: {$default_instruments[$instrument_title]}<br />
                        uniqueMember:<br /><br />
                    ";
                    echo $ldif;
                }
            }
        }
    }

    die();
}

function cms_contacts_update_mailing() {
    $users = CMSContacts::get_or_create_ldap_cache(CMSContacts::$users_cache, 'update_ldap_users_cache');
    $agent = CMSContacts::agent();
    $unidentified_lists = array();
    foreach ($users as $index => $user) {
        if (array_key_exists('mailinglists', $user)) {
            $mailing_lists = $user['mailinglists'];
            CMSUtils::unset_count_key($mailing_lists);
            if ($mailing_lists) {
                foreach ($mailing_lists as $list_name) {
                    $agent->search($agent->base_dn, "(&(objectClass=cmsMailingList)(cn=$list_name))");
                    $lists = $agent->get_entries();
                    CMSUtils::unset_count_key($lists);
                    if ($lists) {
                        foreach ($lists as $count => $list) {
                            $agent->add_to_group($list['dn'], array('uniqueMember' =>  $user['dn']));
                        }
                    }else {
                        $unidentified_lists[] = $list_name;
                    }
                }
            }
        }
    }

    if ($unidentified_lists) {
        echo "<pre>";
        var_dump($unidentified_lists);
        echo "</pre>";
        die();
    }
}

function cms_contacts_update_statuses() {
    $status_person = CMSContacts::get_or_create_ldap_cache(CMSContacts::$status_person_cache, 'update_ldap_taxonomies_cache');
    $status_organization = CMSContacts::get_or_create_ldap_cache(CMSContacts::$status_organization_cache, 'update_ldap_taxonomies_cache');
    $users = CMSContacts::get_or_create_ldap_cache(CMSContacts::$users_cache, 'update_ldap_users_cache');
    $agent = CMSContacts::agent();

    $unidentied_statuses = array(
        'statusPerson' => array(),
        'statusOrganization' => array(),
    );

    foreach ($users as $index => $user) {
        if (array_key_exists('statusperson', $user)) {
            $statuses = $user['statusperson'];
            CMSUtils::unset_count_key($statuses);
            if ($statuses) {
                foreach ($statuses as $status) {
                    $agent->search($agent->base_dn, "(&(objectClass=cmsStatusPerson)(cn=$status))");
                    $ldap_statuses = $agent->get_entries();
                    CMSUtils::unset_count_key($ldap_statuses);
                    if ($ldap_statuses) {
                        foreach ($ldap_statuses as $count => $ldap_status) {
                            $agent->add_to_group($ldap_status['dn'], array('uniqueMember' =>  $user['dn']));
                        }
                    }else {
                        $unidentied_statuses['statusPerson'][] = array(
                            $status,
                            $user['conventions']
                        );
                    }
                }
            }
        }

        if (array_key_exists('statusorganization', $user)) {
            $statuses = $user['statusorganization'];
            CMSUtils::unset_count_key($statuses);
            if ($statuses) {
                foreach ($statuses as $status) {
                    $agent->search($agent->base_dn, "(&(objectClass=cmsStatusOrganization)(cn=$status))");
                    $ldap_statuses = $agent->get_entries();
                    CMSUtils::unset_count_key($ldap_statuses);
                    if ($ldap_statuses) {
                        foreach ($ldap_statuses as $count => $ldap_status) {
                            $agent->add_to_group($ldap_status['dn'], array('uniqueMember' =>  $user['dn']));
                        }
                    }else {
                        $unidentied_statuses['statusOrganization'][] = array(
                            $status,
                            $user['conventions']
                        );
                    }
                }
            }
        }
    }

    if ($unidentied_statuses) {
        echo "<pre>";
        var_dump($unidentied_statuses);
        echo "</pre>";
        die();
    }
}
