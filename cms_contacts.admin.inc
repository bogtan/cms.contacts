<?php

/**
 * Implements hook_admin();
 *
 * Create CMS Contacts administration settings form.
 * The form allows administrators to configure the LDAP server from the
 * Drupal administration interface.
 */
function cms_contacts_admin() {
    $form = array();
    $form[LDAP_ADDRESS] = array(
        '#type' => 'textfield',
        '#title' => t('LDAP Server address'),
        '#description' => t('Server address (e.g. http://example.com/)'),
        '#default_value' => variable_get(LDAP_ADDRESS, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#required' => TRUE,
    );

    $form[LDAP_PORT] = array(
        '#type' => 'textfield',
        '#title' => t('LDAP Server port'),
        '#default_value' => variable_get(LDAP_PORT, '389'),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t(""),
        '#required' => TRUE,
    );

    $form[LDAP_BIND_RDN] = array(
        '#type' => 'textfield',
        '#title' => t('Bind RDN'),
        '#default_value' => variable_get(LDAP_BIND_RDN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('Bind RDN (Relativ distinguished name)'),
        '#required' => TRUE,
    );

    $form[LDAP_PASSWORD] = array(
        '#type' => 'password',
        '#title' => t('LDAP Server password'),
        '#size' => 25,
        '#description' => t(''),
    );

    $form[LDAP_BASE_DN] = array(
        '#type' => 'textfield',
        '#title' => t('Base DN'),
        '#default_value' => variable_get(LDAP_BASE_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('Base DN (Distinguished name)'),
        '#required' => TRUE,
    );

    $form[LDAP_PEOPLE_DN] = array(
        '#type' => 'textfield',
        '#title' => t('People DN'),
        '#default_value' => variable_get(LDAP_PEOPLE_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('LDAP DN for contacts group'),
        '#required' => TRUE,
    );

    $form[LDAP_ORGANIZATION_DN] = array(
        '#type' => 'textfield',
        '#title' => t('Organization DN'),
        '#default_value' => variable_get(LDAP_ORGANIZATION_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('LDAP DN for organizations group'),
        '#required' => TRUE,
    );

    $form[LDAP_DEPARTMENT_DN] = array(
        '#type' => 'textfield',
        '#title' => t('Department DN'),
        '#default_value' => variable_get(LDAP_DEPARTMENT_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('LDAP DN for departments group'),
        '#required' => TRUE,
    );

    return system_settings_form($form);
}

/**
 * Implements hook_admin_validate();
 *
 * Validate CMS Contacts administration settings.
 *
 * @param   array   $form
 * @param   array   $form_state
 */
function cms_contacts_admin_validate($form, &$form_state) {
    $values = $form_state['values'];

    _cms_contacts_check_dn($values[LDAP_BIND_RDN], $base_dn, 'Bind RDN');
    _cms_contacts_check_dn($values[LDAP_PEOPLE_DN], $base_dn, 'People DN');
    _cms_contacts_check_dn($values[LDAP_ORGANIZATION_DN], $base_dn, 'Organization DN');
    _cms_contacts_check_dn($values[LDAP_DEPARTMENT_DN], $base_dn, 'Department DN');
}

/**
 * Check if a given DN (Distinguished name) is correct.
 * The given DN must end with the Base DN.
 *
 * @param   string   $dn
 *    The DN to check
 * @param   string   $base_dn
 *    The Base DN to match
 * @param   string   $dn_name
 *    Name of the DN parameter. Used to set form error.
 *
 * @return  boolean
 */
function _cms_contacts_check_dn($dn, $base_dn, $dn_name) {
    $contains = CMSUtils::ends_with($dn, $base_dn);

    if (!$contains) {
        form_set_error('cms_contacts', t($dn_name . ' must be formed from Base DN'));
    }
}
