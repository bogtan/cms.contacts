<?php

/**
 * @file
 *    Describes functions for Contacts export to CSV and XLS files
 * @author
 *    Bogdan Tanase
 */

module_load_include('inc', 'cms_contacts', 'cms_contacts.pages');

/**
 * Callback function for menu item
 *
 * Exported listed contacts to CSV format
 */
function contacts_export() {
    $instruments = CMSLegalInstrument::get_mapped_instruments();
    $insturments_names = array();
    foreach (array_keys($instruments) as $instrument_name) {
        $instruments_names[strtolower($instrument_name)] = $instrument_name;
    }

    // Connect to LDAP servers
    $agent = CMSContacts::agent();
    if (!$agent->connected) {
        drupal_set_message('Unable to connect to the LDAP server! Please contact site administrator.', 'error');
    }else {
        $filter = _build_contacts_filter();

        // Search LDAP records based on selected filters
        $result = $agent->search($agent->people_dn, $filter);
        if (isset($_GET['iSortCol_0'])) {
            $result = $agent->sort(CMSContacts::$table_columns[$_GET['iSortCol_0']]);
        }

        $all_users = $agent->get_entries();
        unset($all_users['count']);
        if (isset($_GET['sSortDir_0']) && ($_GET['sSortDir_0'] == 'desc')) {
            $all_users = array_reverse($all_users);
        }

        // Offers the file for download
        CMSUtils::set_download_http_headers('text/csv', 'Contacts.csv');
        $fp = fopen('php://output', 'w');

        // Add BOM_UTF-8 header
        fputs($fp, $bom =( chr(0xEF) . chr(0xBB) . chr(0xBF) ));

        // Set file headings
        fputcsv($fp, CMSContacts::$export_file_headings);

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
    $PHPExcel_path = drupal_realpath() . libraries_get_path('PHPExcel') . '/Classes/PHPExcel.php';
    require_once($PHPExcel_path);
//    $library = libraries_load('PHPExcel');
//    if (empty($library['loaded'])) {
//        watchdog('contacts', "Couldn't find the PHPExcel library. Excel export aborted.", array(), WATCHDOG_ERROR);
//        drupal_set_message(t("Couldn't find the PHPExcel library. Please contact the site administrator."), 'error');
//    }

    $xls = new PHPExcel();

    // Set properties
    $xls->getProperties()->setCreator("CMS Databases website");
    $xls->getProperties()->setLastModifiedBy("CMS Databases website");
    $xls->getProperties()->setTitle("CMS Contacts list");
    $xls->getProperties()->setSubject("CMS Contacts list");
    $xls->getProperties()->setDescription("List of all CMS Contacts");

    $xls->setActiveSheetIndex(0);

    $letters = array();
    $total_columns = count(CMSContacts::$export_file_headings);
    $count = 0;
    for ($letter = 'A'; $letter <= 'ZZ'; $letter ++) {
        if ($count < $total_columns) {
            $letters[] = $letter;
        }

        $count ++;
    }

    // Set Sheet column titles
    $count = 0;
    foreach(CMSContacts::$export_file_headings as $tittle) {
        $cell_name = $letters[$count] . "1";
        $count ++;
        $value = $tittle;
        $xls->getActiveSheet()->SetCellValue($cell_name, $value);
        $xls->getActiveSheet()->getStyle($cell_name)->getFont()->setBold(true);
    }

    // Rename sheet
    $xls->getActiveSheet()->setTitle('All contacts');

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
            $result = $agent->sort(CMSContacts::$table_columns[$_GET['iSortCol_0']]);
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
    CMSUtils::set_download_http_headers('application/vnd.ms-excel', 'Contacts.xls');

    $xls_writer = PHPExcel_IOFactory::createWriter($xls, 'Excel5');
    $xls_writer->save('php://output');
}
