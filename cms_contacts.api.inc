<?php

/**
 * @file
 *    Used to handle Contacts webservices
*/

/**
 * Menu callback for /contact/list/terms
 *
 * List Contacts related taxonomy terms from cache
 *
 * @param   string   $website
 *    Website for which to retrieve the taxonomy terms from cache
 * @return  array    $response
*/
function contacts_list_vocabulary_terms_from_cache($website) {
    global $user;
    $response = array();

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
 *
 * List all contacts related taxonomy terms
 * (Mailing lists, Regions, Status person, Status organization)
 *
 * @param   string   $profile
 *    Website from which to retrieve the taxonomy terms from the local cache.
 *    Used to build the relative path to the local cache.
 * @param   string   $type
 *    Get taxonomy terms from cache or directly from the database.
 *    The database retrieve method is used in order to save taxonomy terms in local cache.
 * @return  string   JSON response
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
        $vocabularies = array(VOC_CONTACTS_MAILING, VOC_REGION, VOC_STATUS_ORGANIZATION, VOC_STATUS_PERSON);
        $terms = array();

        foreach ($vocabularies as $vocabulary) {
            $terms[$vocabulary] = CMSUtils::vocabulary_get_terms($vocabulary, FALSE, FALSE);
        }

        echo json_encode($terms);
        drupal_exit();
    }
}

/**
 * Menu callback for /search_contact
 * Contacts API search function
 *
 * Search contacts that contain a given string in their CN (canonical name)
 *
 * @param   string   $string
 * @return  strgin   JSON response
*/
function contacts_search($string = '') {
    if (empty($string)) {
        return '';
    }

    $filter = "(cn=*$string*)";
    $contacts = CMSContacts::get_mapped_contacts_names_uid($filter, TRUE);
    echo json_encode($contacts, JSON_PRETTY_PRINT);
}
