<?php

/**
 * Menu callback for contacts
 *
 * Create an associative array with terms IDs and name
*/
function contacts_list_vocabulary_terms_from_cache($website) {
    global $user;
    $response = array();
    $vocabularies = array();

    $websites = CMSUtils::get_all_websites();
    if (array_key_exists($website, $websites)) {
        $cache_file = drupal_get_path('module', 'cms') . _DS_ . 'remote_cache' . _DS_ . $website . _DS_  . CONTACTS_TAXONOMY_CACHE_FILENAME;
        $vocabularies = file_get_contents($cache_file);
        $response = (array) json_decode($vocabularies);
    }

    return $response;
}

/**
 * Menu callback for api/contacts/terms
*/
function api_contacts_list_terms($profile = '', $type = '') {
    if (empty($profile)) {
        $profile = CMSUtils::get_current_profile();
    }

    if ($type == 'cache') {
        $cache_file = drupal_get_path('module', 'cms') . _DS_ . 'remote_cache' . _DS_ . $profile . _DS_  . CONTACTS_TAXONOMY_CACHE_FILENAME;
        $vocabularies = file_get_contents($cache_file);
        return (array) json_decode($vocabularies);
    }else {
        $vocabularies = array(VOC_CONTACTS_MAILING, VOC_CONTACTS_REGION, VOC_STATUS_ORGANIZATION, VOC_STATUS_PERSON);
        $terms = array();

        foreach ($vocabularies as $vocabulary) {
            $terms[$vocabulary] = CMSUtils::vocabulary_get_terms($vocabulary, FALSE, FALSE);
        }

        echo json_encode($terms);
        drupal_exit();
    }
}

function contacts_search($string = '') {
    $contacts = array();
    if (empty($string)) {
        return '';
    }

    $filter = "(cn=*$string*)";
    $contacts = CMSContacts::get_contacts($filter, TRUE);
    echo json_encode($contacts, JSON_PRETTY_PRINT);
}
