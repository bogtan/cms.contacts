<?php

/**
 * @file
 *    Handle listing of LDAP data, forms' validations and submissions, data export
 */

module_load_include('inc', 'cms_contacts', 'cms_contacts.forms');
module_load_include('inc', 'cms_contacts', 'cms_contacts.api');
module_load_include('inc', 'cms_species', 'cms_species.api');
module_load_include('inc', 'cms_meeting', 'cms_meeting.api');
module_load_include('module', 'cms_instrument', 'cms_instrument');
module_load_include('module', 'cms', 'cms');
module_load_include('module', 'cms_country', 'cms_country');
module_load_include('inc', 'cms_contacts', 'cms_contacts.cache');

define('AVAILABILITY_STATUSES', serialize(array("TRUE", "FALSE")));
define('DEFAULT_CONTACTS_FILTER_OPERATOR', serialize(array("is_all_of", "is_one_of")));


function cms_contacts_get_filters_from_request() {
    $ret = array(
        'instrument' => !empty($_GET['instrument']) ? $_GET['instrument'] : array(),
        'region' => !empty($_GET['region']) ? $_GET['region'] : array(),
        'country' => !empty($_GET['country']) ? $_GET['country'] : array(),
        'person_status' => !empty($_GET['person_status']) ? $_GET['person_status'] : array(),
        'organization_status' => !empty($_GET['organization_status']) ? $_GET['organization_status'] : array(),
        'sSearch' => !empty($_GET['sSearch']) ? $_GET['sSearch'] : FALSE,
        'per_field_operator' => !empty($_GET['per_field_operator']) && $_GET['per_field_operator'] == 'is_one_of',
        'operator' => !empty($_GET['operator']) && $_GET['operator'] == 'is_one_of'
    );
    # Availability has three states
    if(isset($_GET['availability'])) {
        $ret['availability'] = $_GET['availability'] == 'TRUE';
    } else {
        $ret['availability'] = NULL;
    }
    return $ret;
}


function cms_contacts_get_sorting_from_request($default_col = 1) {
    $ret = array();
    $column = $_GET['iSortCol_0'] >= 0 ? intval($_GET['iSortCol_0']) : $default_col;
    $direction = (!empty($_GET['sSortDir_0']) && $_GET['sSortDir_0'] == 'desc') ? 'desc' : 'asc';
    $ret[] = array('column' => $column, 'direction' => $direction);
    return $ret;
}


function cms_contacts_search($filters = array()) {
    $all_users = CMSContactsCache::cache_ldap_users();

    $filter_countries = !empty($filters['country']);
    $filter_regex = $filters['sSearch'] ? '/' . $filters['sSearch'] . '/i' : FALSE;
    $field_or = $filters['per_field_operator'];
    $operator_or = $filters['operator'];

    $filter_instrument = !empty($filters['instrument']);
    $instrument_allowed = array();
    if($filter_instrument) {
        // If we need to filter by instruments, build an array with all the people allowed to be inside the results
        $cached_ip = CMSContactsCache::cache_instrument_people();
        $instrument_allowed = array();
        foreach($cached_ip as $uuid => $user_dn) {
            if(in_array($uuid, $filters['instrument'])) {
                $instrument_allowed = array_merge($instrument_allowed, $user_dn);
            }
        }
        $instrument_allowed = array_unique($instrument_allowed);
    }

    $filter_statusperson = !empty($filters['person_status']);
    $statusperson_allowed = array();
    if($filter_statusperson) {
        // If we need to filter by statusperson, build an array with all the people allowed to be inside the results
        $cached_statusperson = CMSContactsCache::cache_statusperson_people();
        $statusperson_allowed = array();
        foreach($cached_statusperson as $dn => $users) {
            if(in_array($dn, $filters['person_status'])) {
                $statusperson_allowed = array_merge($statusperson_allowed, $users);
            }
        }
        $statusperson_allowed = array_unique($statusperson_allowed);
    }

    $filter_statusorganization = !empty($filters['organization_status']);
    $statusorganization_allowed = array();
    if($filter_statusorganization) {
        // If we need to filter by statusperson, build an array with all the people allowed to be inside the results
        $cached_statusorganization = CMSContactsCache::cache_statusorganization_people();

        $statusorganization_allowed = array();
        foreach($cached_statusorganization as $dn => $users) {
            if(in_array($dn, $filters['organization_status'])) {
                $statusorganization_allowed = array_merge($statusorganization_allowed, $users);
            }
        }
        $statusorganization_allowed = array_unique($statusorganization_allowed);
    }
    $filter_region = !empty($filters['region']);

    $filtered_users = array();
    foreach($all_users as &$u) {
        $add = TRUE;
        # Instrument
        if($filter_instrument) {
            if($field_or) {
                if(!in_array($u->dn, $instrument_allowed)) {
                    $add = FALSE;
                }
            } else {
                if((array_keys($u->instruments) != $filters['instrument'])) {
                    $add = FALSE;
                }
            }
        }
        # Country - $field_or is always TRUE as we have one user per country
        if($filter_countries) {
            $f = $filters['country'];
            if(!array_intersect($f, $u->iso2)) {
                $add = FALSE;
            }
        }
        if($filter_region) {
            if(empty($u->region)) {
                $add = FALSE;
            } else {
                $f = $filters['region'];
                if($field_or) {
                    if(!array_intersect($f, $u->region)) {
                        $add = FALSE;
                    }
                } else {
                    if($f != array_intersect($f, $u->region)) {
                        $add = FALSE;
                    }
                }
            }
        }
        # Availability
        if($filters['availability'] !== NULL && $u->availability != $filters['availability']) {
            $add = FALSE;
        }
        # Search string
        if($filter_regex && !preg_match($filter_regex, $u->filter)) {
            $add = FALSE;
        }
        # Status person
        if($filter_statusperson) {
            if($field_or) {
                if(!in_array($u->dn, $statusperson_allowed)) {
                    $add = FALSE;
                }
            } else {
                // Look upon each group
                foreach($filters['person_status'] as $group_dn) {
                    /** @var $cached_statusperson array */
                    if(!array_key_exists($group_dn, $cached_statusperson) ||
                            !in_array($u->dn, $cached_statusperson[$group_dn])) {
                        $add = FALSE;
                        break;
                    }
                }
            }
        }

        # Status organization
        if($filter_statusorganization) {
            if($field_or) {
                if(!in_array($u->dn, $statusorganization_allowed)) {
                    $add = FALSE;
                }
            } else {
                // Look upon each group
                foreach($filters['organization_status'] as $group_dn) {
                    /** @var $cached_statusorganization array */
                    if(!array_key_exists($group_dn, $cached_statusorganization) ||
                            !in_array($u->dn, $cached_statusorganization[$group_dn])) {
                        $add = FALSE;
                        break;
                    }
                }
            }
        }
        # Assignment
        if($add) {
            $filtered_users[] = $u;
        }
    }
    return $filtered_users;
}


function cms_contacts_sort_results($rows, $criteria = array()) {
    # Sort the results
    if(!empty($criteria)) {
        $column = $criteria[0]['column'];
        $direction = $criteria[0]['direction'];
        usort($rows, function($a, $b) use ($column) {
                switch($column) {
                    case 0:
                        return strcmp($a->givenname, $b->givenname);
                    case 2:
                        return strcmp($a->organization_rendered, $b->organization_rendered);
                    case 3:
                        return strcmp($a->countries_rendered, $b->countries_rendered);
                    case 4:
                        return strcmp($a->countries_post_rendered, $b->countries_post_rendered);
                    case 5:
                        return strcmp($a->instruments_rendered, $b->instruments_rendered);
                    default:
                        return strcmp($a->sn, $b->sn);
                }
            }
        );
        if($direction == 'desc') {
            $rows = array_reverse($rows, FALSE);
        }
    }
    return $rows;
}


function contacts_list_ajax_listing() {
    $filters = cms_contacts_get_filters_from_request();
    $sort = cms_contacts_get_sorting_from_request();
    $users = cms_contacts_search($filters);
    $users = cms_contacts_sort_results($users, $sort);

    $offset = intval($_GET['iDisplayStart']);
    $page_size = intval($_GET['iDisplayLength']);
    if(empty($page_size)) { $page_size = 10; }

    $total = count($users);
    $users = array_slice($users, $offset, $page_size);
    $display = array();
    foreach($users as $row) {
        $display[] = (object)array(
            $row->givenname,
            $row->sn,
            $row->organization,
            $row->countries_rendered,
            $row->countries_post_rendered,
            $row->instruments_rendered,
            $row->edit_link
        );
    }
    $ret = array(
        'sEcho' => intval($_GET['sEcho']),
        'iTotalRecords' => $total,
        'iTotalDisplayRecords' => $total,
        'aaData' => $display
    );
    echo json_encode($ret);
    drupal_exit();
}


/**
 * Render the contacts listing view
 * @return string Rendered HTML template using theme('contacts_list')
 */
function contacts_list() {
    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    drupal_set_breadcrumb($breadcrumb);
    drupal_set_title(t('Contacts'));

    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : array();
    $options = array('species' => array(), 'meetings' => array(), 'regions' => array(), 'mailing' => array(), 'status_organization' => array(), 'status_person' => array());
    _build_contacts_exposed_form_options($selected_instruments, $options);

    $mapped_instruments = CMSLegalInstrument::list_keyed_by_uuid();

    $params = array_merge(
        cms_contacts_get_filters_from_request(),
        array(
            'instrument' => $selected_instruments,
            'countries' => isset($_GET['country']) ? $_GET['country'] : array(),
            'instruments' => $mapped_instruments,
            'regions' => isset($_GET['region']) ? $_GET['region'] : array(),
            'availability' => (isset($_GET['availability']) && (in_array($_GET['availability'], unserialize(AVAILABILITY_STATUSES)))) ? $_GET['availability'] : "",
            'person_status' => isset($_GET['person_status']) ? $_GET['person_status'] : array(),
            'organization_status' => isset($_GET['organization_status']) ? $_GET['organization_status'] : array(),
            'species_options' => $options["species"],
            'meeting_options' => $options["meetings"],
            'region_options' => $options["regions"],
            'org_status_options' => $options["status_organization"],
            'person_status_options' => $options["status_person"],
        )
    );
    return theme('contacts_list', $params);
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

    if (empty($selected_instruments)) {
        foreach (array_keys($instruments) as $instrument_uuid) {
            _update_options($instrument_uuid, $options, $mailing_list, $status_person, $status_organization);
        }
    }else {
        foreach ($selected_instruments as $instrument_uuid) {
            _update_options($instrument_uuid, $options, $mailing_list, $status_person, $status_organization);
        }
    }

    $r = taxonomy_vocabulary_machine_name_load('region');
    $regions = taxonomy_get_tree($r->vid);
    foreach($regions as $t) {
        $options['regions'][$t->name] = $t->name;
    }
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
