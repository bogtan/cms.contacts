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

define('AVAILABILITY_STATUSES', serialize(array("TRUE", "FALSE")));
define('DEFAULT_CONTACTS_FILTER_OPERATOR', serialize(array("is_all_of", "is_one_of")));

/**
 *
 *
 * @return   array
 */
function contacts_list() {
    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    drupal_set_breadcrumb($breadcrumb);

    drupal_set_title(t('Contacts'));

    $groups = CMSUtils::get_mapped_websites();
    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : array();
    $per_page_options = array(10, 25, 50, 100);

    $options = array('species' => array(), 'meetings' => array(), 'regions' => array(), 'mailing' => array(), 'status_organization' => array(), 'status_person' => array());
    _build_contacts_exposed_form_options($selected_instruments, $options);

    $mapped_instruments = array();
    foreach ($groups as $group => $title) {
        $mapped_instruments[CMSUtils::slug($group)] = $title;
    }
    $mapped_instruments = CMSLegalInstrument::cms_instruments_names();

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
    ));
}

/**
 * Callback function for Contacts dataTables serverside processing.
 */
function contacts_datatables_list() {
    if (!CMSUtils::request_is_ajax()) {
        drupal_not_found();
        drupal_exit();
    }

    /**
     * Map LDAP attributes for filtering
     */
    $columns = array(
        'uid',
        'givenname',
        'sn',
        'o',
        'ou',
        'mail',
        'countryEnglish',
        'countryPostName',
        'st',
        'conventions',
    );

    $page = (isset($_GET['iDisplayStart']) && intval($_GET['iDisplayStart'])) ? $_GET['iDisplayStart'] : 1;
    $per_page = (isset($_GET['iDisplayLength']) && intval($_GET['iDisplayLength'])) ? $_GET['iDisplayLength'] : 10;
    $page = $page / $per_page + 1;
    $total_entries = 0;

    $instruments = CMSLegalInstrument::get_mapped_instruments();
    foreach (array_keys($instruments) as $instrument_name) {
        $instruments_names[strtolower($instrument_name)] = $instrument_name;
    }

    /**
     * Connect to LDAP server
     */
    $users = array();
    $agent = CMSContacts::agent();
    if (!$agent->connected) {
        drupal_set_message('Unable to connect to the LDAP server! Please contact site administrator.', 'error');
    }else {
        $filter = _build_contacts_filter();

        /**
         * Search LDAP records based on filter
         */
        $result = $agent->search($agent->people_dn, $filter);

        /**
         * Sort LDAP search results
         */
        if (isset($_GET['iSortCol_0'])) {
            $result = $agent->sort($columns[$_GET['iSortCol_0']]);
        }

        $all_users = $agent->get_entries();
        $total_entries = $all_users['count'];

        /**
         * Remove 'count' element from users results;
         * If sort direction is descending we don't want to include 'count' too
         */
        unset($all_users['count']);

        if (isset($_GET['sSortDir_0']) && ($_GET['sSortDir_0'] == 'desc')) {
            $all_users = array_reverse($all_users);
        }

        /**
         * Page search results
         */
        $paged_entries = array_chunk($all_users, $per_page);
        $users = $paged_entries[$page - 1];
        $total_pages = ceil($total_entries / $per_page);
    }

    /**
     * Build data for dataTables
     */
    $aaData = array();
    foreach ($users as $index => $user) {
        $aaData[$index] = array(
            @$user['uid'][0],
            @$user['givenname'][0],
            @$user['sn'][0],
            @$user['o'][0],
            @$user['ou'][0],
            @$user['mail'],
        );

        if (isset($user['iso2'])) {
            if ($user['iso2']['count'] > 1) {
                $countries = "";
                foreach ($user['iso2'] as $index => $country) {
                    if (is_numeric($index)) {
                        $countries .= country_load($user['iso2'][$index])->name;
                        if ($index < $user['iso2']['count']) {
                            $countries .= ", ";
                        }
                    }
                }

                $aaData[$index][] = $countries;
            }else {
                $country = country_load($user['iso2'][0])->name;
                $aaData[$index][] = $country;
            }
        }else {
            $aaData[$index][] = '';
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

                $aaData[$index][] = $countries;
            }else {
                $country = country_load($user['cpiso2'][0])->name;
                $aaData[$index][] = $country;
            }
        }else {
            $aaData[$index][] = '';
        }

        $main_instrument = '';
        $aaData[$index][] = (isset($user['st']) && !empty($user['st'])) ? $user['st'][0] : '';
        if (isset($user['conventions'])) {
            if ($user['conventions']['count'] > 1) {
                $conventions = "";
                foreach ($user['conventions'] as $convention_index => $convention) {
                    if ((is_numeric($convention_index)) && (isset($instruments_names[$convention]))) {
                        if ($main_instrument == '') {
                            $main_instrument = $convention;
                        }

                        $conventions .= $instruments_names[$convention];
                        if (($convention_index + 1) < $user['conventions']['count']) {
                            $conventions .= ", ";
                        }
                    }
                }
            }else {
                $convention = $user['conventions'][0];
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
 * Callback function for Contacts dataTables serverside processing.
 */
function organizations_datatables_list() {
    /**
     * Map LDAP attributes for filtering
     */
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

    /**
     * Connect to LDAP server
     */
    $agent = CMSContacts::agent();
    if (!$agent->connected) {
        drupal_set_message('Unable to connect to the LDAP server! Please contact site administrator.', 'error');
    }else {
        $filter = _build_contacts_filter('cmsOrganization');

        /**
         * Search LDAP records based on filter
         */
        $result = $agent->search($agent->organization_dn, $filter);

        /**
         * Sort LDAP search results
         */
        if (isset($_GET['iSortCol_0'])) {
            $result = $agent->sort($columns[$_GET['iSortCol_0']]);
        }

        $all_organizations = $agent->get_entries();
        $total_entries = $all_organizations['count'];

        /**
         * Remove 'count' element from users results;
         * If sort direction is descending we don't want to include 'count' too
         */
        unset($all_organizations['count']);

        if (isset($_GET['sSortDir_0']) && ($_GET['sSortDir_0'] == 'desc')) {
            $all_organizations = array_reverse($all_organizations);
        }

        /**
         * Page search results
         */
        $paged_entries = array_chunk($all_organizations, $per_page);
        $organizations = $paged_entries[$page - 1];
        $total_pages = ceil($total_entries / $per_page);
    }

    /**
     * Build data for dataTables
     */
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
 * Callback function for menu item
 *
 * Exported listed contacts to CSV format
 */
function contacts_export() {
    $columns = array(
        'uid',
        'givenname',
        'sn',
        'o',
        'ou',
        'mail',
        'iso2',
        'cpiso2',
        'st',
        'conventions',
    );

    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : array();

    $page = (isset($_GET['iDisplayStart']) && intval($_GET['iDisplayStart'])) ? $_GET['iDisplayStart'] : 1;
    $per_page = (isset($_GET['iDisplayLength']) && intval($_GET['iDisplayLength'])) ? $_GET['iDisplayLength'] : 10;
    $page = $page / $per_page + 1;

    $instruments = CMSLegalInstrument::get_mapped_instruments();
    $insturments_names = array();
    foreach (array_keys($instruments) as $instrument_name) {
        $instruments_names[strtolower($instrument_name)] = $instrument_name;
    }

    /**
     * Connect to LDAP server
     */
    $agent = CMSContacts::agent();
    if (!$agent->connected) {
        drupal_set_message('Unable to connect to the LDAP server! Please contact site administrator.', 'error');
    }else {
        $filter = _build_contacts_filter();

        /**
         * Search LDAP records based on filter
         */
        $result = $agent->search($agent->people_dn, $filter);
        if (isset($_GET['iSortCol_0'])) {
            $result = $agent->sort($columns[$_GET['iSortCol_0']]);
        }

        $all_users = $agent->get_entries();
        unset($all_users['count']);
        if (isset($_GET['sSortDir_0']) && ($_GET['sSortDir_0'] == 'desc')) {
            $all_users = array_reverse($all_users);
        }

        drupal_add_http_header('Pragma', 'public');
        drupal_add_http_header('Expires', '0');
        drupal_add_http_header('Cache-Control', 'must-revalidate, post-check=0, pre-check=0');
        drupal_add_http_header('Content-Description', 'File transfer');
        drupal_add_http_header('Content-Type', 'text/csv; charset=utf-8');
        drupal_add_http_header('Content-Disposition', 'attachment; filename=contacts.csv');
        drupal_add_http_header('Content-Transfer-Encoding', 'binary');
        $fp = fopen('php://output', 'w');

        /**
         * Add BOM_UTF-8 header
         */
        fputs($fp, $bom =( chr(0xEF) . chr(0xBB) . chr(0xBF) ));

        /**
         * CSV file headers
         */
        $headers = array(
            //'ID',
            //'UID',
            'Availability',
            'Personal title',
            'First name',
            'Last name',
            'Position',
            'Status person',
            'Organization',
            'Department',
            'Status organization',
            'Website',
            'Address',
            'Town/City',
            'Postal Code',
            'Country',
            'Region',
            'Country Post',
            'Work phone',
            'Mobile',
            'Fax',
            'Work email',
            'Personal e-mail',
            'Additional e-mails',
            'Home phone',
            'Skype',
            'CMS Instruments',
            'Mailing lists',
            'Description',
            'Preferred language',
            'Meetings',
            'Species',
        );
        fputcsv($fp, $headers);

        foreach($all_users as $index => $user) {
            $data = array();
            _prepare_contact_details($data, $user);
            fputcsv($fp, $data);
        }

        fclose($fp);
        drupal_exit();
    }
}

/**
 * Callback function for /contacts/xls_export
 *
 * Export Contacts table in XLS file using PHPExcel library
 */
function contacts_xls_export() {
    $library = libraries_load('PHPExcel');
    if (empty($library['loaded'])) {
        watchdog('contacts', "Couldn't find the PHPExcel library. Excel export aborted.", array(), WATCHDOG_ERROR);
        drupal_set_message(t("Couldn't find the PHPExcel library. Please contact the site administrator."), 'error');
    }

    $xls = new PHPExcel();

    // Set properties
    $xls->getProperties()->setCreator("CMS Databases website");
    $xls->getProperties()->setLastModifiedBy("CMS Databases website");
    $xls->getProperties()->setTitle("CMS Contacts list");
    $xls->getProperties()->setSubject("CMS Contacts list");
    $xls->getProperties()->setDescription("List of all CMS Contacts");

    // Add some data
    $headers = array(
        //'ID',
        //'UID',
        'Availability',
        'Personal title',
        'First name',
        'Last name',
        'Position',
        'Status person',
        'Organization',
        'Department',
        'Status organization',
        'Website',
        'Address',
        'Town/City',
        'Postal Code',
        'Country',
        'Region',
        'Country Post',
        'Work phone',
        'Mobile',
        'Fax',
        'Work email',
        'Personal e-mail',
        'Additional e-mails',
        'Home phone',
        'Skype',
        'CMS Instruments',
        'Mailing lists',
        'Description',
        'Preferred language',
        'Meetings',
        'Species',
    );

    $xls->setActiveSheetIndex(0);

    $letters = array();
    $total_columns = count($headers);
    $count = 0;
    for ($letter = 'A'; $letter <= 'ZZ'; $letter ++) {
        if ($count < $total_columns) {
            $letters[] = $letter;
        }

        $count ++;
    }

    // Set Sheet column titles
    $count = 0;
    foreach($headers as $tittle) {
        $cell_name = $letters[$count] . "1";
        $count ++;
        $value = $tittle;
        $xls->getActiveSheet()->SetCellValue($cell_name, $value);
        $xls->getActiveSheet()->getStyle($cell_name)->getFont()->setBold(true);
    }

    // Rename sheet
    $xls->getActiveSheet()->setTitle('All contacts');

    $columns = array(
        'uid',
        'givenname',
        'sn',
        'o',
        'ou',
        'mail',
        'iso2',
        'cpiso2',
        'st',
        'conventions',
    );

    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : array();

    $page = (isset($_GET['iDisplayStart']) && intval($_GET['iDisplayStart'])) ? $_GET['iDisplayStart'] : 1;
    $per_page = (isset($_GET['iDisplayLength']) && intval($_GET['iDisplayLength'])) ? $_GET['iDisplayLength'] : 10;
    $page = $page / $per_page + 1;

    $instruments = CMSLegalInstrument::get_mapped_instruments();
    $insturments_names = array();
    foreach (array_keys($instruments) as $instrument_name) {
        $instruments_names[strtolower($instrument_name)] = $instrument_name;
    }

    /**
     * Connect to LDAP server
     */
    $agent = CMSContacts::agent();
    if (!$agent->connected) {
        drupal_set_message('Unable to connect to the LDAP server! Please contact site administrator.', 'error');
    }else {
        $filter = _build_contacts_filter();

        // Search and sort LDAP records based on filter
        $result = $agent->search($agent->people_dn, $filter);
        if (isset($_GET['iSortCol_0'])) {
            $result = $agent->sort($columns[$_GET['iSortCol_0']]);
        }

        $all_users = $agent->get_entries();
        unset($all_users['count']);

        // Set sorting direction
        if (isset($_GET['sSortDir_0']) && ($_GET['sSortDir_0'] == 'desc')) {
            $all_users = array_reverse($all_users);
        }

        // Write each user's data to the XLS file
        foreach($all_users as $index => $user) {
            $cell_name = "A" . ($index + 2);
            $data = array();
            _prepare_contact_details($data, $user);
            $xls->getActiveSheet()->fromArray($data, NULL, $cell_name);
        }
    }


    // Redirect output to a client’s web browser (Excel5)
    drupal_add_http_header('Content-Type', 'application/vnd.ms-excel');
    drupal_add_http_header('Content-Disposition', 'attachment;filename="contacts.xls'); // @todo Let user choose the filename
    drupal_add_http_header('Content-Transfer-Encoding', 'binary');
    drupal_add_http_header('Cache-Control', 'max-age=0');
    drupal_add_http_header('Expires', 'Mon, 26 Jul 1997 05:00:00 GMT');
    drupal_add_http_header('Last-Modified', gmdate('D, d M Y H:i:s').' GMT');
    drupal_add_http_header('Cache-Control', 'cache, must-revalidate');
    drupal_add_http_header('Pragma', 'public');

    $xls_writer = PHPExcel_IOFactory::createWriter($xls, 'Excel5');
    $xls_writer->save('php://output');
}

/**
 * Callback function for menu item.
 *
 * Show contact details of a specified UID
 */
function contacts_view($contact_id, $group) {
    $agent = CMSContacts::agent();

    $contact = $agent->search_user($contact_id);
    $exists = CMSContacts::user_in_group($group, $contact, $agent);

    if (empty($contact) || (!$exists)) {
        drupal_not_found();
        drupal_exit();
    }

    $page_title = $contact['givenname'][0] . ' ' . $contact['sn'][0];
    drupal_set_title($page_title, PASS_THROUGH);

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');
    drupal_set_breadcrumb($breadcrumb);

    $contact['instrument'] = $group;
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
    $agent = CMSContacts::agent();
    $edit = (isset($form['#contact']) && (!empty($form['#contact']))) ? TRUE : FALSE;

    $user_data = CMSContacts::prepare_user_data($form_state['values'], $edit);

    if ((isset($form['#contact']) && (!empty($form['#contact'])))) {
        $user_id = $form['#contact']['uid'][0];
    }else {
        $user_id = $user_data['uid'][0];
    }

    $contact_link = l(t('view'), 'contacts/items/' . $user_id . '/view');
    $watchdog_args = array('@type' => 'Contact', '%title' => $user_data['sn'][0] . ' ' . $user_data['givenName'][0]);
    $t_args = array('@type' => 'Contact', '%title' => $user_data['sn'][0] . ' ' . $user_data['givenName'][0]);

    if ($edit) {
        // Edit form
        $user = $form['#contact'];
        if (isset($user['o']) && (!empty($user['o']))) {
            $organization = CMSContacts::get_or_create_organization($user['o'][0], $user['conventions']);
            $department = CMSContacts::get_or_create_department($user['ou'][0], $user['conventions']);
        }

        if (!$agent->edit($user['dn'], $user_data)) {
            drupal_set_message(t('The contact could not be saved.'), 'error');
            $form_state['rebuild'] = TRUE;
        }else {
            CMSContacts::set_contact_related_content($user_id, $form_state['values']['species'], 'species', 'field_species_experts');
            CMSContacts::set_contact_related_content($user_id, $form_state['values']['meetings'], 'meeting', 'field_meeting_participants');
            CMSContacts::update_user_groups($user_id, $user_data['conventions'], $agent);
            watchdog('content', '@type: updated %title.', $watchdog_args, WATCHDOG_NOTICE, $contact_link);
            drupal_set_message(t('@type %title has been updated.', $t_args));

            /**
             * Redirect to contact page
             */
            $semaphore = variable_get('cron_semaphore', FALSE);

            if (!$semaphore) {
                drupal_goto('/contacts/item/' . $user_id . '/' . $user_data['conventions'][0] . '/view');
            }

        }
    }else {
        // Add form
        $dn = "uid=" . $user_id . "," . $agent->people_dn;
        if(!$agent->add($dn, $user_data)) {
            drupal_set_message(t('The contact could not be saved.'), 'error');
            $form_state['rebuild'] = TRUE;
        }else {
            $current_profile = CMSUtils::get_current_profile();

            if ($current_profile !== 'cms') {
                $groups[] = $current_profile;
            }else {
                $groups = $user_data['conventions'];
            }

            foreach ($groups as $index => $group) {
                if (!CMSContacts::group_exists($group, $agent)) {
                    $agent->create_group($group, "cn=$group,ou=Users,o=CMS", array($dn));
                }else {
                    $agent->add_to_group($group, array('uniqueMember' => $dn));
                }
            }

            CMSContacts::set_contact_related_content($user_id, $form_state['values']['species'], 'species', 'field_species_experts');
            watchdog('content', '@type: added %title.', $watchdog_args, WATCHDOG_NOTICE, $contact_link);
            drupal_set_message(t('@type %title has been created.', $t_args));

            /**
             * Redirect to contact page
             */
            $semaphore = variable_get('cron_semaphore', FALSE);

            if (!$semaphore) {
                drupal_goto('/contacts/item/' . $user_id . '/' . $user_data['conventions'][0] . '/view');
            }
        }
    }

    if ($user_id) {
        $form_state['values']['uid'] = $user_id;
        $form_state['uid'] = $user_id;
        $form_state['redirect'] = 'contacts/listing';
    }else {
        drupal_set_message(t('The contact could not be saved.'), 'error');
    }

    // Clear the page and block caches.
    cache_clear_all();
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

            /**
             * Redirect to contact page
             */
            $semaphore = variable_get('cron_semaphore', FALSE);

            if (!$semaphore) {
                drupal_goto('contacts/organization/' . $oid . '/' . $organization_data['conventions'][0] . '/view');
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

            /**
             * Redirect to contact page
             */
            $semaphore = variable_get('cron_semaphore', FALSE);

            if (!$semaphore) {
                drupal_goto('contacts/organization/' . $oid . '/' . $organization_data['conventions'][0] . '/view');
            }
        }
    }

    if ($oid) {
        $form_state['values']['oid'] = $oid;
        $form_state['oid'] = $oid;
        $form_state['redirect'] = 'contacts/organizations/listing';
    }else {
        drupal_set_message(t('The organization could not be saved.'), 'error');
    }

    // Clear the page and block caches.
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
function contacts_edit($contact_id, $group) {
    $current_profile = CMSUtils::get_current_profile();
    if ($current_profile != 'cms') {
        $group = $current_profile;
    }

    $agent = CMSContacts::agent();
    $contact = $agent->search_user($contact_id);
    $exists = CMSContacts::user_in_group($group, $contact, $agent);

    if (empty($contact) || (!$exists)) {
        drupal_not_found();
        drupal_exit();
    }

    $breadcrumb = array();
    $breadcrumb[] = l(t('Home'), '<front>');
    $breadcrumb[] = l(t('Contacts'), ADMINISTRATION_PATH . 'contacts/listing');
    $breadcrumb[] = l($contact['givenname'][0] . ' ' . $contact['sn'][0], ADMINISTRATION_PATH . 'contacts/item/' . $contact_id . '/' . $group . '/view');

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
    $current_profile = CMSUtils::get_current_profile();
    if ($current_profile != 'cms') {
        $group = $current_profile;
    }

    $agent = CMSContacts::agent();
    $contact = $agent->search_user($contact_id);
    $exists = CMSContacts::user_in_group($group, $contact, $agent);

    if (empty($contact) || (!$exists)) {
        drupal_not_found();
        drupal_exit();
    }

    $full_name = $contact['givenname'][0] . ' ' . $contact['sn'][0];
    $form['#user'] = $contact;
    $form['#group'] = $group;
    $form['user_id'] = array('#type' => 'value', '#value' => $contact_id);

    return confirm_form($form,
        t('Are you sure you want to delete %user contact?', array('%user' => $full_name)),
        'contacts/item/' . $contact_id . '/' . $group . '/view',
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
        $group = $form['#group'];
        $current_profile = CMSUtils::get_current_profile();
        if ($current_profile != 'cms') {
            $group = $current_profile;
        }

        $agent = CMSContacts::agent();
        $contact = $agent->search_user($contact_id);
        $exists = CMSContacts::user_in_group($group, $contact, $agent);

        if (empty($contact) || (!$exists)) {
            drupal_not_found();
            drupal_exit();
        }

        $full_name = $contact['givenname'][0] . ' ' . $full_name = $contact['sn'][0];
        $dn = "uid=$contact_id," . $agent->people_dn;
        $agent->delete($dn);

        /**
         * Delete user from all assigned groups
         */
        if ($contact['conventions']['count']) {
            foreach ($contact['conventions'] as $index => $group_name) {
                if (is_numeric($index)) {
                    @$agent->delete_from_group($group_name, array('uniquemember' => $dn));
                }
            }
        }

        watchdog('content', '@type: deleted %full_name.', array('@type' => 'Contact', '%full_name' => $full_name));
        drupal_set_message(t('@type %full_name has been deleted.', array('@type' => 'Contact', '%full_name' => $full_name)));
    }

    $form_state['redirect'] = 'contacts/listing';
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

    $form_state['redirect'] = 'contacts/organizations/listing';
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
                        /**
                         * Check if user already has assigned the instrument
                         */
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
        drupal_goto('/contacts/listing');
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
 *
 * @param   array   $options
 */
function _build_contacts_exposed_form_options($selected_instruments, &$options) {
    $websites = CMSUtils::get_all_websites();

    if (empty($selected_instruments) || ($selected_instruments == 'null') || ($selected_instruments[0] == '')) {
        foreach ($websites as $name => $website) {
            $remote_species = api_species_list('species_from_cache', $name);
            $options['species'] = array_merge($options['species'], $remote_species);
            $remote_meetings = api_meeting_list('cache', $name);
            $options['meetings'] = array_merge($options['meetings'], $remote_meetings);
            $vocabularies = contacts_list_vocabulary_terms_from_cache($name);
            $options['mailing'] = build_terms_options($vocabularies, $options['mailing'], VOC_CONTACTS_MAILING, $name, $website['title']);
            $options['regions'] = build_terms_options($vocabularies, $options['regions'], VOC_REGION, $name, $website['title']);
            $options['status_organization'] = build_terms_options($vocabularies, $options['status_organization'], VOC_STATUS_ORGANIZATION, $name, $website['title']);
            $options['status_person'] = build_terms_options($vocabularies, $options['status_person'], VOC_STATUS_PERSON, $name, $website['title']);
        }
    }else {
        foreach ($selected_instruments as $instrument) {
            $title = isset($websites[$instrument]) ? $websites[$instrument]['title'] : '';
            $options["species"] = api_species_list('species_from_cache', $instrument);
            $options["meetings"] = api_meeting_list('cache', $instrument);
            $vocabularies = contacts_list_vocabulary_terms_from_cache($instrument);
            $options["mailing"] = build_terms_options($vocabularies, $options["mailing"], VOC_CONTACTS_MAILING, $instrument, $title);
            $options["regions"] = build_terms_options($vocabularies, $options["regions"], VOC_REGION, $instrument, $title);
            $options["status_organization"] = build_terms_options($vocabularies, $options["status_organization"], VOC_STATUS_ORGANIZATION, $instrument, $title);
            $options["status_person"] = build_terms_options($vocabularies, $options["status_person"], VOC_STATUS_PERSON, $instrument, $title);
        }
    }
}


/**
 * Build LDAP filter for Contacts
 *
 * @param   string   $profile   Convention to look for
 * @return  strgin   $filter
 */
function _build_contacts_filter($objectClass = 'cmsContact') {
    $selected_operator = (isset($_GET['operator']) && in_array($_GET['operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['operator'] : 'is_all_of';
    $selected_per_field_operator = (isset($_GET['per_field_operator']) && in_array($_GET['per_field_operator'], unserialize(DEFAULT_CONTACTS_FILTER_OPERATOR))) ? $_GET['per_field_operator'] : 'is_all_of';
    $countries = isset($_GET['country']) ? $_GET['country'] : '';
    $regions = isset($_GET['region']) ? $_GET['region'] : '';
    $mailing_lists = isset($_GET['mailing_list']) ? $_GET['mailing_list'] : '';
    $availability = (isset($_GET['availability']) && (in_array($_GET['availability'], unserialize(AVAILABILITY_STATUSES)))) ? $_GET['availability'] : "";
    $person_statuses = isset($_GET['person_status']) ? $_GET['person_status'] : '';
    $organization_statuses = isset($_GET['organization_status']) ? $_GET['organization_status'] : '';
    $species = isset($_GET['species']) ? $_GET['species'] : '';
    $meeting = isset($_GET['meeting']) ? $_GET['meeting'] : '';
    $search_query = (isset($_GET['sSearch']) && !empty($_GET['sSearch'])) ? $_GET['sSearch'] : '';
    $selected_instruments = isset($_GET['instrument']) ? $_GET['instrument'] : '';
    if (!empty($selected_instruments) && ($selected_instruments != 'null')) {
        $selected_instruments = explode(',', $selected_instruments);
    }

    $species_options = $meeting_options = $region_options = $mailing_options = $org_status_options = $person_status_options = array();
    $options = array('species', 'meetings', 'regions', 'mailing', 'status_organization', 'status_person');
    _build_contacts_exposed_form_options($selected_instruments, $options);

    $operator = ($selected_operator == 'is_all_of') ? '&' : '|';
    $per_field_operator = ($selected_per_field_operator == 'is_all_of') ? '&' : '|';

    if (!empty($selected_instruments) && ($selected_instruments != 'null')) {
        if (count($selected_instruments) > 1) {
            $filter = "(" . $per_field_operator;
            foreach ($selected_instruments as $instrument) {
                $filter .= "(conventions=" . $instrument . ")";
            }
            $filter .= ")";
        }else {
            $filter = "(conventions=" . $selected_instruments[0] . ")";
        }
    }else {
        $filter = "(conventions=*)";
    }

    if (!empty($regions) || (!empty($countries) && ($countries != 'null')) || !empty($search_query) || !empty($availability) || (!empty($mailing_lists) && ($mailing_lists != 'null')) ||
        !empty($organization_statuses) || !empty($person_statuses) || (!empty($species) && ($species != 'null'))) {
        $filter = "(" . $operator . $filter;

        if (!empty($countries) && ($countries != 'null')) {
            $countries = explode(',', $countries);
            $filter .= "(" . $per_field_operator;
            foreach($countries as $country) {
                $filter .= "(|(iso2=" . strtolower($country) . ")(cpIso2=" . strtolower($country) . "))";
            }
            $filter .= ")";
        }

        if (!empty($regions) && ($regions != 'null')) {
            $regions = explode(',', $regions);
            $filter .= "(" . $per_field_operator;
            foreach ($regions as $region) {
                if (array_key_exists($region, $region_options)) {
                    $filter .= "(region=" . $region_options[$region] . ")";
                }
            }
            $filter .= ")";
        }

        if (!empty($availability)) {
            $filter .= "(cmsAvailability=" . $availability . ")";
        }

        if (!empty($mailing_lists) && ($mailing_lists != 'null')) {
            $mailing_lists = explode(',', $mailing_lists);
            $filter .= "(" . $per_field_operator;
            foreach ($mailing_lists as $mailing_list) {
                if (array_key_exists($mailing_list, $mailing_options)) {
                    $filter .= "(mailingLists=" . $mailing_options[$mailing_list] . ")";
                }
            }
            $filter .= ")";
        }

        if (!empty($person_statuses) && ($person_statuses != 'null')) {
            $person_statuses = explode(',', $person_statuses);
            $filter .= "(" . $per_field_operator;
            foreach ($person_statuses as $person_status) {
                if (array_key_exists($person_status, $person_status_options)) {
                    $person_status = preg_replace('/([\(\)])/',"\\\\$1", $person_status_options[$person_status]);
                    $filter .= "(statusPerson=" . $person_status . ")";
                }
            }
            $filter .= ")";
        }

        if (!empty($organization_statuses) && ($organization_statuses != 'null')) {
            $organization_statuses = explode(',', $organization_statuses);
            $filter .= "(" . $per_field_operator;
            foreach ($organization_statuses as $organization_status) {
                if (array_key_exists($organization_status, $org_status_options)) {
                    $organization_status = preg_replace('/([\(\)])/',"\\\\$1", $org_status_options[$organization_status]);
                    $filter .= "(statusOrganization=" . $organization_status . ")";
                }
            }

            $filter .= ")";
        }

        if (array_key_exists($species, $species_options)) {
            $experts = $species_options[$species]->experts_dns;
            if (!empty($experts)) {
                $filter .= "(" . $per_field_operator;
                foreach ($experts as $index => $expert) {
                    $filter .= "(uid=" . $expert . ")";
                }
                $filter .= ")";
            }
        }

        if (!empty($search_query)) {
            $filter .= "(|(sn=*" . $search_query . "*)(givenName=*" . $search_query . "*)(o=*" . $search_query . "*)(ou=*" . $search_query . "*)(st=*" . $search_query . "*)(mail=*" . $search_query . "*)(additionalEmails=*" . $search_query . "*)(personalEmail=*" . $search_query . "*)(website=*" . $search_query . "*))";
        }

        $filter .= ")";
    }

    $filter = "(&(objectClass=" . $objectClass . ")" . $filter . ")";
    return $filter;
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
